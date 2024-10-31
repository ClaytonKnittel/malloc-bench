#include "src/correctness_checker.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "absl/algorithm/container.h"
#include "absl/container/btree_map.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/synchronization/mutex.h"
#include "util/absl_util.h"

#include "src/allocator_interface.h"
#include "src/heap_factory.h"
#include "src/rng.h"
#include "src/tracefile_executor.h"
#include "src/tracefile_reader.h"

namespace bench {

/* static */
bool CorrectnessChecker::IsFailedTestStatus(const absl::Status& status) {
  return status.message().starts_with(kFailedTestPrefix);
}

/* static */
absl::Status CorrectnessChecker::Check(
    TracefileReader& reader, HeapFactory& heap_factory, bool verbose,
    const TracefileExecutorOptions& options) {
  absl::btree_map<void*, uint32_t> allocated_blocks;

  CorrectnessChecker checker(reader, heap_factory);
  checker.verbose_ = verbose;
  return checker.Run(options);
}

CorrectnessChecker::CorrectnessChecker(TracefileReader& reader,
                                       HeapFactory& heap_factory)
    : TracefileExecutor(reader, heap_factory),
      heap_factory_(&heap_factory),
      rng_(0, 1) {}

void CorrectnessChecker::InitializeHeap(HeapFactory& heap_factory) {
  heap_factory.Reset();
  initialize_heap(heap_factory);
}

absl::StatusOr<void*> CorrectnessChecker::Malloc(
    size_t size, std::optional<size_t> alignment) {
  return Alloc(1, size, alignment.value_or(0), /*is_calloc=*/false);
}

absl::StatusOr<void*> CorrectnessChecker::Calloc(size_t nmemb, size_t size) {
  return Alloc(nmemb, size, /*alignment=*/0, /*is_calloc=*/true);
}

absl::StatusOr<void*> CorrectnessChecker::Realloc(void* ptr, size_t size) {
  if (ptr == nullptr) {
    if (verbose_) {
      std::cout << "realloc(nullptr, " << size << ")" << std::endl;
    }

    void* new_ptr = bench::realloc(nullptr, size);
    RETURN_IF_ERROR(HandleNewAllocation(new_ptr, size, /*alignment=*/0,
                                        /*is_calloc=*/false));
    return new_ptr;
  }

  AllocatedBlock block;
  size_t orig_size;
  {
    absl::MutexLock lock(&mutex_);
    auto block_it = allocated_blocks_.find(ptr);
    if (block_it == allocated_blocks_.end()) {
      return absl::InternalError(absl::StrFormat(
          "%s realloc-ed block %p not found in allocated blocks map",
          kFailedTestPrefix, ptr));
    }
    block = block_it->second;
    orig_size = block.size;
  }

  if (verbose_) {
    std::cout << "realloc(" << ptr << ", " << size << ")" << std::endl;
  }

  // Check that the block has not been corrupted.
  RETURN_IF_ERROR(CheckMagicBytes(ptr, orig_size, block.magic_bytes));

  void* new_ptr = bench::realloc(ptr, size);

  if (size == 0) {
    if (new_ptr != nullptr) {
      return absl::InternalError(
          absl::StrFormat("%s Expected `nullptr` return value on realloc with "
                          "size 0: %p = realloc(%p, %zu)",
                          kFailedTestPrefix, new_ptr, ptr, size));
    }

    absl::MutexLock lock(&mutex_);
    allocated_blocks_.erase(ptr);
    return new_ptr;
  }
  if (new_ptr != ptr) {
    mutex_.Lock();
    size_t erased_elems = allocated_blocks_.erase(ptr);
    mutex_.Unlock();
    if (erased_elems != 1) {
      return absl::InternalError(absl::StrFormat(
          "%s realloc-ed block %p not found in allocated blocks map",
          kFailedTestPrefix, ptr));
    }

    RETURN_IF_ERROR(ValidateNewBlock(new_ptr, size, /*alignment=*/0));

    block.size = size;
    mutex_.Lock();
    auto [it, inserted] = allocated_blocks_.insert({ new_ptr, block });
    mutex_.Unlock();
    if (!inserted) {
      return absl::InternalError(
          absl::StrFormat("%s realloc-ed block %p of size %zu conflicts with "
                          "existing allocation",
                          kFailedTestPrefix, ptr, size));
    }
  } else {
    absl::MutexLock lock(&mutex_);
    auto block_it = allocated_blocks_.find(ptr);
    if (block_it == allocated_blocks_.end()) {
      return absl::InternalError(absl::StrFormat(
          "%s realloc-ed block %p not found in allocated blocks map",
          kFailedTestPrefix, ptr));
    }
    block_it->second.size = size;
  }

  RETURN_IF_ERROR(
      CheckMagicBytes(new_ptr, std::min(orig_size, size), block.magic_bytes));
  if (size > orig_size) {
    FillMagicBytes(new_ptr, size, block.magic_bytes);
  }

  return new_ptr;
}

absl::Status CorrectnessChecker::Free(void* ptr,
                                      std::optional<size_t> size_hint,
                                      std::optional<size_t> alignment_hint) {
  if (ptr == nullptr) {
    free(nullptr);
    return absl::OkStatus();
  }

  AllocatedBlock block;
  {
    absl::MutexLock lock(&mutex_);
    auto block_it = allocated_blocks_.find(ptr);
    if (block_it == allocated_blocks_.end()) {
      return absl::InternalError(
          absl::StrFormat("%s freed block %p not found in allocated blocks map",
                          kFailedTestPrefix, ptr));
    }
    block = block_it->second;
  }

  if (verbose_) {
    std::cout << "free(" << ptr << ")" << std::endl;
  }

  // Check that the block has not been corrupted.
  RETURN_IF_ERROR(CheckMagicBytes(ptr, block.size, block.magic_bytes));

  bench::free(ptr, size_hint.value_or(0), alignment_hint.value_or(0));

  mutex_.Lock();
  size_t erased_elems = allocated_blocks_.erase(ptr);
  mutex_.Unlock();
  if (erased_elems != 1) {
    return absl::InternalError(
        absl::StrFormat("%s freed block %p not found in allocated blocks map",
                        kFailedTestPrefix, ptr));
  }

  return absl::OkStatus();
}

absl::StatusOr<void*> CorrectnessChecker::Alloc(size_t nmemb, size_t size,
                                                size_t alignment,
                                                bool is_calloc) {
  if (verbose_) {
    if (is_calloc) {
      std::cout << "calloc(" << nmemb << ", " << size << ")" << std::endl;
    } else if (alignment != 0) {
      std::cout << "aligned_alloc(" << size << ", " << alignment << ")"
                << std::endl;
    } else {
      std::cout << "malloc(" << size << ")" << std::endl;
    }
  }

  void* ptr;
  if (is_calloc) {
    ptr = bench::calloc(nmemb, size);
  } else {
    ptr = bench::malloc(nmemb * size, alignment);
  }
  size *= nmemb;

  if (size == 0) {
    if (ptr != nullptr) {
      return absl::InternalError(
          absl::StrFormat("%s Expected `nullptr` return value on malloc with "
                          "size 0: %p = malloc(%zu)",
                          kFailedTestPrefix, ptr, size));
    }

    return ptr;
  }

  RETURN_IF_ERROR(HandleNewAllocation(ptr, size, alignment, is_calloc));
  return ptr;
}

absl::Status CorrectnessChecker::HandleNewAllocation(void* ptr, size_t size,
                                                     size_t alignment,
                                                     bool is_calloc) {
  RETURN_IF_ERROR(ValidateNewBlock(ptr, size, alignment));

  uint64_t magic_bytes;
  {
    absl::MutexLock lock(&mutex_);
    magic_bytes = rng_.GenRand64();
    allocated_blocks_.insert({
        ptr,
        AllocatedBlock{
            .size = size,
            .magic_bytes = magic_bytes,
        },
    });
  }

  if (is_calloc) {
    for (size_t i = 0; i < size; i++) {
      if (static_cast<uint8_t*>(ptr)[i] != 0x00) {
        return absl::InternalError(absl::StrFormat(
            "%s calloc-ed block at %p of size %zu is not cleared",
            kFailedTestPrefix, ptr, size));
      }
    }
  }

  FillMagicBytes(ptr, size, magic_bytes);
  return absl::OkStatus();
}

absl::Status CorrectnessChecker::ValidateNewBlock(void* ptr, size_t size,
                                                  size_t alignment) {
  if (ptr == nullptr) {
    return absl::InternalError(absl::StrFormat(
        "%s Bad nullptr alloc for size %zu, did you run out of memory?",
        kFailedTestPrefix, size));
  }

  RETURN_IF_ERROR(heap_factory_->WithInstances<absl::Status>(
      [ptr, size](const auto& instances) -> absl::Status {
        if (absl::c_any_of(instances, [ptr, size](const auto& heap) {
              return ptr >= heap->Start() &&
                     static_cast<uint8_t*>(ptr) + size <= heap->End();
            })) {
          return absl::OkStatus();
        }

        std::string heaps;
        for (const auto& heap : instances) {
          if (!heaps.empty()) {
            heaps += ", ";
          }
          heaps += absl::StrFormat("%p-%p", heap->Start(), heap->End());
        }

        return absl::InternalError(absl::StrFormat(
            "%s Bad alloc of out-of-range block at %p of size %zu, "
            "heaps range from %v",
            kFailedTestPrefix, ptr, size, heaps));
      }));

  auto block = FindContainingBlock(ptr);
  if (block.has_value()) {
    return absl::InternalError(absl::StrFormat(
        "%s Bad alloc of %p within allocated block at %p of size %zu",
        kFailedTestPrefix, ptr, block.value()->first,
        block.value()->second.size));
  }

  size_t ptr_val = static_cast<char*>(ptr) - static_cast<char*>(nullptr);
  const size_t min_alignment = size <= 8 ? 8 : 16;
  alignment = std::max(alignment, min_alignment);
  if (ptr_val % alignment != 0) {
    return absl::InternalError(
        absl::StrFormat("%s Pointer %p of size %zu is not aligned to %zu bytes",
                        kFailedTestPrefix, ptr, size, alignment));
  }

  return absl::OkStatus();
}

/* static */
void CorrectnessChecker::FillMagicBytes(void* ptr, size_t size,
                                        uint64_t magic_bytes) {
  size_t i;
  for (i = 0; i < size / 8; i++) {
    static_cast<uint64_t*>(ptr)[i] = magic_bytes;
  }
  for (size_t j = 8 * i; j < size; j++) {
    static_cast<uint8_t*>(ptr)[j] = magic_bytes >> (8 * (j - 8 * i));
  }
}

/* static */
absl::Status CorrectnessChecker::CheckMagicBytes(void* ptr, size_t size,
                                                 uint64_t magic_bytes) {
  size_t i;
  for (i = 0; i < size / 8; i++) {
    uint64_t val = static_cast<uint64_t*>(ptr)[i];
    if (val != magic_bytes) {
      size_t offset = i * 8 + (std::countr_zero(val ^ magic_bytes) / 8);
      return absl::InternalError(absl::StrFormat(
          "%s Allocated block %p of size %zu has dirtied bytes at "
          "position %zu from the beginning",
          kFailedTestPrefix, ptr, size, offset));
    }
  }
  for (size_t j = 8 * i; j < size; j++) {
    if (static_cast<uint8_t*>(ptr)[j] !=
        static_cast<uint8_t>(magic_bytes >> (8 * (j - 8 * i)))) {
      return absl::InternalError(absl::StrFormat(
          "%s Allocated block %p of size %zu has dirtied bytes at "
          "position %zu from the beginning",
          kFailedTestPrefix, ptr, size, j));
    }
  }

  return absl::OkStatus();
}

std::optional<typename CorrectnessChecker::Map::const_iterator>
CorrectnessChecker::FindContainingBlock(void* ptr) {
  absl::MutexLock lock(&mutex_);
  auto it = allocated_blocks_.upper_bound(ptr);
  if (it != allocated_blocks_.begin() && it != allocated_blocks_.end()) {
    --it;
  }
  if (it != allocated_blocks_.end()) {
    // Check if the block contains `ptr`.
    if (it->first <= ptr &&
        ptr < static_cast<char*>(it->first) + it->second.size) {
      return it;
    }
  }
  return std::nullopt;
}

}  // namespace bench

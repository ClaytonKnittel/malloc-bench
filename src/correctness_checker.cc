#include "src/correctness_checker.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "absl/algorithm/container.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "folly/Random.h"
#include "util/absl_util.h"

#include "src/heap_factory.h"
#include "src/malloc_runner.h"
#include "src/tracefile_executor.h"
#include "src/tracefile_reader.h"

namespace bench {

CorrectnessChecker::CorrectnessChecker(HeapFactory& heap_factory, bool verbose)
    : MallocRunner(heap_factory, MallocRunnerOptions{ .verbose = verbose }),
      heap_factory_(&heap_factory) {}

/* static */
absl::Status CorrectnessChecker::Check(
    TracefileReader& reader, HeapFactory& heap_factory, bool verbose,
    const TracefileExecutorOptions& options) {
  absl::btree_map<void*, uint32_t> allocated_blocks;

  TracefileExecutor<CorrectnessChecker> checker(reader, std::ref(heap_factory),
                                                verbose);
  return checker.Run(options);
}

absl::Status CorrectnessChecker::PostAlloc(void* ptr, size_t size,
                                           std::optional<size_t> alignment,
                                           bool is_calloc) {
  if (size == 0) {
    return absl::OkStatus();
  }

  RETURN_IF_ERROR(ValidateNewBlock(ptr, size, alignment));

  uint64_t magic_bytes = static_cast<uint64_t>(folly::ThreadLocalPRNG()());
  auto [it, inserted] = allocated_blocks_.insert({
      ptr,
      AllocatedBlock{
          .size = size,
          .magic_bytes = magic_bytes,
      },
  });
  if (!inserted) {
    return absl::InternalError(
        absl::StrFormat("%s Duplicate allocation %p, requested size %zu, but "
                        "already exists and is of size %zu",
                        kFailedTestPrefix, ptr, size, it->second.size));
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

absl::StatusOr<AllocatedBlock> CorrectnessChecker::PreRealloc(void* ptr,
                                                              size_t size) {
  (void) size;
  auto block_it = allocated_blocks_.find(ptr);
  if (block_it == allocated_blocks_.end()) {
    return absl::InternalError(absl::StrFormat(
        "%s realloc-ed block %p not found in allocated blocks map",
        kFailedTestPrefix, ptr));
  }
  AllocatedBlock block = block_it->second;

  // Check that the block has not been corrupted.
  RETURN_IF_ERROR(CheckMagicBytes(ptr, block.size, block.magic_bytes));

  size_t erased_elems = allocated_blocks_.erase(ptr);
  if (erased_elems != 1) {
    return absl::InternalError(absl::StrFormat(
        "%s realloc-ed block %p not found in allocated blocks map",
        kFailedTestPrefix, ptr));
  }

  return block;
}

absl::Status CorrectnessChecker::PostRealloc(void* new_ptr, void* old_ptr,
                                             size_t size,
                                             AllocatedBlock block) {
  RETURN_IF_ERROR(ValidateNewBlock(new_ptr, size, /*alignment=*/0));

  const size_t orig_size = block.size;
  block.size = size;

  auto [it, inserted] = allocated_blocks_.insert({ new_ptr, block });
  if (!inserted) {
    return absl::InternalError(
        absl::StrFormat("%s realloc-ed block %p of size %zu conflicts with "
                        "existing allocation",
                        kFailedTestPrefix, old_ptr, size));
  }

  RETURN_IF_ERROR(
      CheckMagicBytes(new_ptr, std::min(orig_size, size), block.magic_bytes));
  if (size > orig_size) {
    FillMagicBytes(new_ptr, size, block.magic_bytes);
  }

  return absl::OkStatus();
}

absl::Status CorrectnessChecker::PreRelease(void* ptr) {
  if (ptr == nullptr) {
    return absl::OkStatus();
  }

  auto block_it = allocated_blocks_.find(ptr);
  if (block_it == allocated_blocks_.end()) {
    return absl::InternalError(
        absl::StrFormat("%s freed block %p not found in allocated blocks map",
                        kFailedTestPrefix, ptr));
  }
  AllocatedBlock block = block_it->second;

  // Check that the block has not been corrupted.
  RETURN_IF_ERROR(CheckMagicBytes(ptr, block.size, block.magic_bytes));

  size_t erased_elems = allocated_blocks_.erase(ptr);
  if (erased_elems != 1) {
    return absl::InternalError(
        absl::StrFormat("%s freed block %p not found in allocated blocks map",
                        kFailedTestPrefix, ptr));
  }

  return absl::OkStatus();
}

absl::Status CorrectnessChecker::ValidateNewBlock(
    void* ptr, size_t size, std::optional<size_t> alignment) const {
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

  // TODO: replicate this logic with magic bytes map lookup.
  // auto block = FindContainingBlock(ptr);
  // if (block.has_value()) {
  //   return absl::InternalError(absl::StrFormat(
  //       "%s Bad alloc of %p within allocated block at %p of size %zu",
  //       kFailedTestPrefix, ptr, block.value()->first,
  //       block.value()->second.size));
  // }

  if (alignment.has_value()) {
    size_t ptr_val = static_cast<char*>(ptr) - static_cast<char*>(nullptr);
    const size_t min_alignment = size <= 8 ? 8 : 16;
    alignment = std::max(alignment.value(), min_alignment);
    if (ptr_val % alignment.value() != 0) {
      return absl::InternalError(absl::StrFormat(
          "%s Pointer %p of size %zu is not aligned to %zu bytes",
          kFailedTestPrefix, ptr, size, alignment.value()));
    }
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

}  // namespace bench

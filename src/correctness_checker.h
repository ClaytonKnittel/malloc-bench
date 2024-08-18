#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "util/absl_util.h"

#include "src/allocator_interface.h"
#include "src/common.h"
#include "src/rng.h"
#include "src/singleton_heap.h"
#include "src/tracefile_reader.h"

namespace bench {

template <MallocInterface I>
class CorrectnessCheckerImpl {
 public:
  static constexpr std::string kFailedTestPrefix = "[Failed]";

  static bool IsFailedTestStatus(const absl::Status& status);

  static absl::Status Check(const std::string& tracefile, bool verbose = false);

 private:
  struct AllocatedBlock {
    size_t size;
    uint64_t magic_bytes;
  };

  using Map = absl::btree_map<void*, AllocatedBlock>;
  using IdMap = absl::flat_hash_map<void*, void*>;

  explicit CorrectnessCheckerImpl(TracefileReader&& reader);

  absl::Status Run();
  absl::Status ProcessTracefile();

  absl::Status Malloc(size_t nmemb, size_t size, void* id, bool is_calloc);
  absl::Status Realloc(void* orig_id, size_t size, void* new_id);
  absl::Status Free(void* id);

  absl::Status HandleNewAllocation(void* id, void* ptr, size_t size,
                                   bool is_calloc);

  // Validates that a new block doesn't overlap with any existing block, and
  // that it satisfies alignment requirements.
  absl::Status ValidateNewBlock(void* ptr, size_t size) const;

  static void FillMagicBytes(void* ptr, size_t size, uint64_t magic_bytes);
  // Checks if pointer is filled with magic_bytes, and if any byte differs,
  // returns the offset from the beginning of the block of the first differing
  // byte.
  static absl::Status CheckMagicBytes(void* ptr, size_t size,
                                      uint64_t magic_bytes);

  std::optional<typename Map::const_iterator> FindContainingBlock(
      void* ptr) const;

  TracefileReader reader_;

  SingletonHeap* heap_;

  Map allocated_blocks_;
  // A map from the pointer value ID from the trace to the actual pointer value
  // allocated by the allocator.
  IdMap id_map_;

  util::Rng rng_;

  bool verbose_;
};

/* static */
template <MallocInterface I>
bool CorrectnessCheckerImpl<I>::IsFailedTestStatus(const absl::Status& status) {
  return status.message().starts_with(kFailedTestPrefix);
}

/* static */
template <MallocInterface I>
absl::Status CorrectnessCheckerImpl<I>::Check(const std::string& tracefile,
                                              bool verbose) {
  absl::btree_map<void*, uint32_t> allocated_blocks;

  DEFINE_OR_RETURN(TracefileReader, reader, TracefileReader::Open(tracefile));

  CorrectnessCheckerImpl checker(std::move(reader));
  checker.verbose_ = verbose;
  return checker.Run();
}

template <MallocInterface I>
CorrectnessCheckerImpl<I>::CorrectnessCheckerImpl(TracefileReader&& reader)
    : reader_(std::move(reader)), rng_(0, 1) {}

template <MallocInterface I>
absl::Status CorrectnessCheckerImpl<I>::Run() {
  SingletonHeap* heap = SingletonHeap::GlobalInstance();
  if (heap == nullptr) {
    return absl::InternalError("`HeapManager()` returned `nullptr` heap.");
  }
  heap_ = heap;
  heap->Reset();
  I::initialize_heap();

  absl::Status result = ProcessTracefile();
  heap->Reset();
  I::initialize_heap();
  heap_ = nullptr;
  return result;
}

template <MallocInterface I>
absl::Status CorrectnessCheckerImpl<I>::ProcessTracefile() {
  while (true) {
    DEFINE_OR_RETURN(std::optional<TraceLine>, line, reader_.NextLine());
    if (!line.has_value()) {
      break;
    }

    switch (line->op) {
      case TraceLine::Op::kMalloc:
        RETURN_IF_ERROR(
            Malloc(1, line->input_size, line->result, /*is_calloc=*/false));
        break;
      case TraceLine::Op::kCalloc:
        RETURN_IF_ERROR(Malloc(line->nmemb, line->input_size, line->result,
                               /*is_calloc=*/true));
        break;
      case TraceLine::Op::kRealloc:
        RETURN_IF_ERROR(
            Realloc(line->input_ptr, line->input_size, line->result));
        break;
      case TraceLine::Op::kFree:
        RETURN_IF_ERROR(Free(line->input_ptr));
        break;
    }
  }

  return absl::OkStatus();
}

template <MallocInterface I>
absl::Status CorrectnessCheckerImpl<I>::Malloc(size_t nmemb, size_t size,
                                               void* id, bool is_calloc) {
  if (id_map_.contains(id)) {
    return absl::InternalError(
        absl::StrFormat("Unexpected duplicate ID allocated: %p", id));
  }

  if (verbose_) {
    if (is_calloc) {
      std::cout << "calloc(" << nmemb << ", " << size << ")" << std::endl;
    } else {
      std::cout << "malloc(" << size << ")" << std::endl;
    }
  }

  void* ptr;
  if (is_calloc) {
    ptr = I::calloc(nmemb, size);
  } else {
    ptr = I::malloc(nmemb * size);
  }
  size *= nmemb;

  if (size == 0) {
    if (id != nullptr) {
      return absl::InternalError(
          "Unexpected non-null new_id for malloc with size 0");
    }

    if (ptr != nullptr) {
      return absl::InternalError(
          absl::StrFormat("%s Expected `nullptr` return value on malloc with "
                          "size 0: %p = malloc(%zu)",
                          kFailedTestPrefix, ptr, size));
    }

    return absl::OkStatus();
  }

  return HandleNewAllocation(id, ptr, size, is_calloc);
}

template <MallocInterface I>
absl::Status CorrectnessCheckerImpl<I>::Realloc(void* orig_id, size_t size,
                                                void* new_id) {
  if (orig_id == nullptr) {
    if (verbose_) {
      std::cout << "realloc(nullptr, " << size << ")" << std::endl;
    }

    void* new_ptr = I::realloc(nullptr, size);
    return HandleNewAllocation(new_id, new_ptr, size, /*is_calloc=*/false);
  }

  auto id_map_it = id_map_.find(orig_id);
  if (id_map_it == id_map_.end()) {
    return absl::InternalError(absl::StrFormat(
        "Unexpected realloc of unknown pointer ID: %p", orig_id));
  }
  void* ptr = id_map_it->second;
  auto block_it = allocated_blocks_.find(ptr);
  AllocatedBlock block = block_it->second;
  size_t orig_size = block.size;

  if (verbose_) {
    std::cout << "realloc(" << ptr << ", " << size << ")" << std::endl;
  }

  if (new_id != orig_id && id_map_.contains(new_id)) {
    return absl::InternalError(
        absl::StrFormat("Unexpected duplicate ID reallocated: %p", new_id));
  }

  // Check that the block has not been corrupted.
  RETURN_IF_ERROR(CheckMagicBytes(ptr, orig_size, block.magic_bytes));

  void* new_ptr = I::realloc(ptr, size);

  if (size == 0) {
    if (new_id != nullptr) {
      return absl::InternalError(
          "Unexpected non-null new_id for realloc with size 0");
    }

    if (new_ptr != nullptr) {
      return absl::InternalError(
          absl::StrFormat("%s Expected `nullptr` return value on realloc with "
                          "size 0: %p = realloc(%p, %zu)",
                          kFailedTestPrefix, new_ptr, ptr, size));
    }

    allocated_blocks_.erase(block_it);
    id_map_.erase(id_map_it);
    return absl::OkStatus();
  }
  if (new_ptr != ptr) {
    allocated_blocks_.erase(block_it);

    RETURN_IF_ERROR(ValidateNewBlock(new_ptr, size));

    block.size = size;
    block_it = allocated_blocks_.insert({ new_ptr, block }).first;
  } else {
    block_it->second.size = size;
  }

  if (orig_id != new_id) {
    id_map_.erase(id_map_it);
    id_map_[new_id] = new_ptr;
  }

  RETURN_IF_ERROR(
      CheckMagicBytes(new_ptr, std::min(orig_size, size), block.magic_bytes));
  if (size > orig_size) {
    FillMagicBytes(new_ptr, size, block.magic_bytes);
  }

  return absl::OkStatus();
}

template <MallocInterface I>
absl::Status CorrectnessCheckerImpl<I>::Free(void* id) {
  if (id == nullptr) {
    I::free(nullptr);
    return absl::OkStatus();
  }

  auto id_map_it = id_map_.find(id);
  if (id_map_it == id_map_.end()) {
    return absl::InternalError(
        absl::StrFormat("Unexpected free of unknown pointer ID: %p", id));
  }
  void* ptr = id_map_it->second;
  auto block_it = allocated_blocks_.find(ptr);

  if (verbose_) {
    std::cout << "free(" << ptr << ")" << std::endl;
  }

  // Check that the block has not been corrupted.
  RETURN_IF_ERROR(CheckMagicBytes(ptr, block_it->second.size,
                                  block_it->second.magic_bytes));

  I::free(ptr);

  id_map_.erase(id_map_it);
  allocated_blocks_.erase(block_it);
  return absl::OkStatus();
}

template <MallocInterface I>
absl::Status CorrectnessCheckerImpl<I>::HandleNewAllocation(void* id, void* ptr,
                                                            size_t size,
                                                            bool is_calloc) {
  RETURN_IF_ERROR(ValidateNewBlock(ptr, size));

  uint64_t magic_bytes = rng_.GenRand64();
  allocated_blocks_.insert({
      ptr,
      AllocatedBlock{
          .size = size,
          .magic_bytes = magic_bytes,
      },
  });

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
  id_map_[id] = ptr;
  return absl::OkStatus();
}

template <MallocInterface I>
absl::Status CorrectnessCheckerImpl<I>::ValidateNewBlock(void* ptr,
                                                         size_t size) const {
  if (ptr == nullptr) {
    return absl::InternalError(absl::StrFormat(
        "%s Bad nullptr alloc for size %zu, did you run out of memory?",
        kFailedTestPrefix, size));
  }
  if (ptr < heap_->Start() ||
      static_cast<uint8_t*>(ptr) + size > heap_->End()) {
    return absl::InternalError(absl::StrFormat(
        "%s Bad alloc of out-of-range block at %p of size %zu "
        "(heap ranges from %p to %p)",
        kFailedTestPrefix, ptr, size, heap_->Start(), heap_->End()));
  }

  auto block = FindContainingBlock(ptr);
  if (block.has_value()) {
    return absl::InternalError(absl::StrFormat(
        "%s Bad alloc of %p within allocated block at %p of size %zu",
        kFailedTestPrefix, ptr, block.value()->first,
        block.value()->second.size));
  }

  size_t ptr_val = static_cast<char*>(ptr) - static_cast<char*>(nullptr);
  if (size <= 8 && ptr_val % 8 != 0) {
    return absl::InternalError(
        absl::StrFormat("%s Pointer %p of size %zu is not aligned to 8 bytes",
                        kFailedTestPrefix, ptr, size));
  }
  if (size > 8 && ptr_val % 16 != 0) {
    return absl::InternalError(
        absl::StrFormat("%s Pointer %p of size %zu is not aligned to 16 bytes",
                        kFailedTestPrefix, ptr, size));
  }

  return absl::OkStatus();
}

/* static */
template <MallocInterface I>
void CorrectnessCheckerImpl<I>::FillMagicBytes(void* ptr, size_t size,
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
template <MallocInterface I>
absl::Status CorrectnessCheckerImpl<I>::CheckMagicBytes(void* ptr, size_t size,
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

template <MallocInterface I>
std::optional<typename CorrectnessCheckerImpl<I>::Map::const_iterator>
CorrectnessCheckerImpl<I>::FindContainingBlock(void* ptr) const {
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

using CorrectnessChecker = CorrectnessCheckerImpl<CkMallocInterface>;

}  // namespace bench

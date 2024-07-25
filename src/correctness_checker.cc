#include "src/correctness_checker.h"

#include <bit>
#include <cstddef>
#include <cstdint>

#include "absl/container/btree_map.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"

#include "src/allocator_interface.h"
#include "src/rng.h"
#include "src/tracefile_reader.h"
#include "src/util.h"

namespace bench {

/* static */
absl::Status CorrectnessChecker::Check(const std::string& tracefile) {
  absl::btree_map<void*, uint32_t> allocated_blocks;

  DEFINE_OR_RETURN(TracefileReader, reader, TracefileReader::Open(tracefile));

  CorrectnessChecker checker(std::move(reader));
  return checker.Run();
}

CorrectnessChecker::CorrectnessChecker(TracefileReader&& reader)
    : reader_(std::move(reader)), rng_(0, 1) {}

absl::Status CorrectnessChecker::Run() {
  std::optional<TraceLine> line;
  while ((line = reader_.NextLine()).has_value()) {
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

absl::Status CorrectnessChecker::Malloc(size_t nmemb, size_t size, void* id,
                                        bool is_calloc) {
  if (id_map_.contains(id)) {
    return absl::InternalError(
        absl::StrFormat("Unexpected duplicate ID allocated: %p", id));
  }

  void* ptr;
  if (is_calloc) {
    ptr = bench::calloc(nmemb, size);
  } else {
    ptr = bench::malloc(nmemb * size);
  }
  size *= nmemb;

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
            "calloc-ed block at %p of size %zu is not cleared", ptr, size));
      }
    }
  }

  FillMagicBytes(ptr, size, magic_bytes);
  id_map_[id] = ptr;
  return absl::OkStatus();
}

absl::Status CorrectnessChecker::Realloc(void* orig_id, size_t size,
                                         void* new_id) {
  auto id_map_it = id_map_.find(orig_id);
  if (id_map_it == id_map_.end()) {
    return absl::InternalError(absl::StrFormat(
        "Unexpected realloc of unknown pointer ID: %p", orig_id));
  }
  void* ptr = id_map_it->second;
  auto block_it = allocated_blocks_.find(ptr);
  AllocatedBlock block = block_it->second;
  size_t orig_size = block.size;

  if (new_id != orig_id && id_map_.contains(new_id)) {
    return absl::InternalError(
        absl::StrFormat("Unexpected duplicate ID reallocated: %p", new_id));
  }

  void* new_ptr = bench::realloc(ptr, size);

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

  RETURN_IF_ERROR(CheckMagicBytes(new_ptr, std::min(orig_size, size),
                                  block_it->second.magic_bytes));

  return absl::OkStatus();
}

absl::Status CorrectnessChecker::Free(void* id) {
  auto id_map_it = id_map_.find(id);
  if (id_map_it == id_map_.end()) {
    return absl::InternalError(
        absl::StrFormat("Unexpected free of unknown pointer ID: %p", id));
  }
  void* ptr = id_map_it->second;
  auto block_it = allocated_blocks_.find(ptr);

  // Check that the block has not been corrupted.
  RETURN_IF_ERROR(CheckMagicBytes(ptr, block_it->second.size,
                                  block_it->second.magic_bytes));

  bench::free(ptr);
  id_map_.erase(id_map_it);
  allocated_blocks_.erase(block_it);
  return absl::OkStatus();
}

absl::Status CorrectnessChecker::ValidateNewBlock(void* ptr,
                                                  size_t size) const {
  auto block = FindContainingBlock(ptr);
  if (block.has_value()) {
    return absl::InternalError(absl::StrFormat(
        "Bad alloc of %p within allocated block at %p of size %zu", ptr,
        block.value()->first, block.value()->second.size));
  }

  size_t ptr_val = static_cast<char*>(ptr) - static_cast<char*>(nullptr);
  if (size <= 8 && ptr_val % 8 != 0) {
    return absl::InternalError(absl::StrFormat(
        "Pointer %p of size %zu is not aligned to 8 bytes", ptr, size));
  }
  if (size > 8 && ptr_val % 16 != 0) {
    return absl::InternalError(absl::StrFormat(
        "Pointer %p of size %zu is not aligned to 16 bytes", ptr, size));
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
      return absl::InternalError(
          absl::StrFormat("Allocated block %p of size %zu has dirtied bytes at "
                          "position %zu from the beginning",
                          ptr, size, offset));
    }
  }
  for (size_t j = 8 * i; j < size; j++) {
    if (static_cast<uint8_t*>(ptr)[j] != (magic_bytes >> (8 * (j - 8 * i)))) {
      return absl::InternalError(
          absl::StrFormat("Allocated block %p of size %zu has dirtied bytes at "
                          "position %zu from the beginning",
                          ptr, size, j));
    }
  }

  return absl::OkStatus();
}

std::optional<CorrectnessChecker::Map::const_iterator>
CorrectnessChecker::FindContainingBlock(void* ptr) const {
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

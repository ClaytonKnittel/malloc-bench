#include "src/correctness_checker.h"

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
            Malloc(line->input_size, line->result, /*is_calloc=*/false));
        break;
      case TraceLine::Op::kCalloc:
        RETURN_IF_ERROR(
            Malloc(line->input_size, line->result, /*is_calloc=*/true));
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

absl::Status CorrectnessChecker::Malloc(size_t size, void* id, bool is_calloc) {
  if (id_map_.contains(id)) {
    return absl::InternalError(
        absl::StrFormat("Unexpected duplicate ID allocated: %p", id));
  }

  void* res = bench::malloc(size);
  auto block = FindContainingBlock(res);
  if (block.has_value()) {
    return absl::InternalError(absl::StrFormat(
        "Bad alloc of %p within allocated block at %p of size %zu", res,
        block.value()->first, block.value()->second.size));
  }

  size_t ptr_val = static_cast<char*>(res) - static_cast<char*>(nullptr);
  if (size <= 8 && ptr_val % 8 != 0) {
    return absl::InternalError(absl::StrFormat(
        "Pointer %p of size %zu is not aligned to 8 bytes", res, size));
  }
  if (size > 8 && ptr_val % 16 != 0) {
    return absl::InternalError(absl::StrFormat(
        "Pointer %p of size %zu is not aligned to 16 bytes", res, size));
  }

  uint64_t magic_bytes = rng_.GenRand64();
  allocated_blocks_.insert({
      res,
      AllocatedBlock{
          .size = size,
          .magic_bytes = magic_bytes,
      },
  });

  if (is_calloc) {
    for (size_t i = 0; i < size; i++) {
      if (static_cast<uint8_t*>(res)[i] != 0x00) {
        return absl::InternalError(absl::StrFormat(
            "calloc-ed block at %p of size %zu is not cleared", res, size));
      }
    }
  }

  size_t i;
  for (i = 0; i < size / 8; i++) {
    static_cast<uint64_t*>(res)[i] = magic_bytes;
  }
  for (size_t j = 8 * i; j < size; j++) {
    static_cast<uint8_t*>(res)[j] = magic_bytes >> (8 * (j - 8 * i));
  }

  id_map_[id] = res;

  return absl::OkStatus();
}

absl::Status CorrectnessChecker::Realloc(void* orig_id, size_t size,
                                         void* new_id) {}

absl::Status CorrectnessChecker::Free(void* id) {}

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

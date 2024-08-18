#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "absl/container/btree_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"

#include "src/heap_interface.h"
#include "src/rng.h"
#include "src/tracefile_executor.h"
#include "src/tracefile_reader.h"

namespace bench {

class CorrectnessChecker : private TracefileExecutor {
 public:
  static constexpr std::string kFailedTestPrefix = "[Failed]";

  static bool IsFailedTestStatus(const absl::Status& status);

  static absl::Status Check(const std::string& tracefile, Heap* heap,
                            bool verbose = false);

  void InitializeHeap() override;
  absl::StatusOr<void*> Malloc(size_t size) override;
  absl::StatusOr<void*> Calloc(size_t nmemb, size_t size) override;
  absl::StatusOr<void*> Realloc(void* ptr, size_t size) override;
  absl::Status Free(void* ptr) override;

 private:
  struct AllocatedBlock {
    size_t size;
    uint64_t magic_bytes;
  };

  using Map = absl::btree_map<void*, AllocatedBlock>;

  CorrectnessChecker(TracefileReader&& reader, Heap* heap);

  absl::StatusOr<void*> Alloc(size_t nmemb, size_t size, bool is_calloc);

  absl::Status HandleNewAllocation(void* ptr, size_t size, bool is_calloc);

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

  Heap* heap_;

  Map allocated_blocks_;

  util::Rng rng_;

  bool verbose_;
};

}  // namespace bench

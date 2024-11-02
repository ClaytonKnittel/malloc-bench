#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "folly/AtomicHashMap.h"

#include "src/heap_factory.h"
#include "src/tracefile_executor.h"
#include "src/tracefile_reader.h"

namespace bench {

class CorrectnessChecker : private TracefileExecutor {
 public:
  static constexpr char kFailedTestPrefix[] = "[Failed]";

  static bool IsFailedTestStatus(const absl::Status& status);

  static absl::Status Check(
      TracefileReader& reader, HeapFactory& heap_factory, bool verbose = false,
      const TracefileExecutorOptions& options = TracefileExecutorOptions());

 private:
  struct AllocatedBlock {
    size_t size;
    uint64_t magic_bytes;
  };

  using BlockMap = folly::AtomicHashMap<void*, AllocatedBlock>;

  CorrectnessChecker(TracefileReader& reader, HeapFactory& heap_factory);

  void InitializeHeap(HeapFactory& heap_factory) override;
  absl::StatusOr<void*> Malloc(size_t size,
                               std::optional<size_t> alignment) override;
  absl::StatusOr<void*> Calloc(size_t nmemb, size_t size) override;
  absl::StatusOr<void*> Realloc(void* ptr, size_t size) override;
  absl::Status Free(void* ptr, std::optional<size_t> size_hint,
                    std::optional<size_t> alignment_hint) override;

  absl::StatusOr<void*> Alloc(size_t nmemb, size_t size, size_t alignment,
                              bool is_calloc);

  absl::Status HandleNewAllocation(void* ptr, size_t size, size_t alignment,
                                   bool is_calloc);

  // Validates that a new block doesn't overlap with any existing block, and
  // that it satisfies alignment requirements.
  absl::Status ValidateNewBlock(void* ptr, size_t size, size_t alignment) const;

  static void FillMagicBytes(void* ptr, size_t size, uint64_t magic_bytes);
  // Checks if pointer is filled with magic_bytes, and if any byte differs,
  // returns the offset from the beginning of the block of the first differing
  // byte.
  static absl::Status CheckMagicBytes(void* ptr, size_t size,
                                      uint64_t magic_bytes);

  HeapFactory* const heap_factory_;

  BlockMap allocated_blocks_;

  bool verbose_;
};

}  // namespace bench

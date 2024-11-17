#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "folly/concurrency/ConcurrentHashMap.h"

#include "src/heap_factory.h"
#include "src/malloc_runner.h"
#include "src/tracefile_executor.h"
#include "src/tracefile_reader.h"

namespace bench {

struct AllocatedBlock {
  size_t size;
  uint64_t magic_bytes;
};

class CorrectnessChecker : public MallocRunner<AllocatedBlock> {
 public:
  explicit CorrectnessChecker(HeapFactory& heap_factory, bool verbose);

  static absl::Status Check(
      TracefileReader& reader, HeapFactory& heap_factory, bool verbose = false,
      const TracefileExecutorOptions& options = TracefileExecutorOptions());

  absl::Status PostAlloc(void* ptr, size_t size,
                         std::optional<size_t> alignment,
                         bool is_calloc) override;
  absl::StatusOr<AllocatedBlock> PreRealloc(void* ptr, size_t size) override;
  absl::Status PostRealloc(void* new_ptr, void* old_ptr, size_t size,
                           AllocatedBlock prev_block) override;
  absl::Status PreRelease(void* ptr) override;

 private:
  using BlockMap = folly::ConcurrentHashMap<void*, AllocatedBlock>;

  // Validates that a new block doesn't overlap with any existing block, and
  // that it satisfies alignment requirements.
  absl::Status ValidateNewBlock(void* ptr, size_t size,
                                std::optional<size_t> alignment) const;

  static void FillMagicBytes(void* ptr, size_t size, uint64_t magic_bytes);
  // Checks if pointer is filled with magic_bytes, and if any byte differs,
  // returns the offset from the beginning of the block of the first differing
  // byte.
  static absl::Status CheckMagicBytes(void* ptr, size_t size,
                                      uint64_t magic_bytes);

  HeapFactory* const heap_factory_;

  BlockMap allocated_blocks_;
};

}  // namespace bench

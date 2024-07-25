#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"

#include "src/fake_heap.h"
#include "src/rng.h"
#include "src/tracefile_reader.h"

namespace bench {

class CorrectnessChecker {
 public:
  static absl::Status Check(const std::string& tracefile, bool verbose = false);

 private:
  struct AllocatedBlock {
    size_t size;
    uint64_t magic_bytes;
  };

  using Map = absl::btree_map<void*, AllocatedBlock>;
  using IdMap = absl::flat_hash_map<void*, void*>;

  explicit CorrectnessChecker(TracefileReader&& reader);

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

  std::optional<Map::const_iterator> FindContainingBlock(void* ptr) const;

  TracefileReader reader_;

  FakeHeap* heap_;

  Map allocated_blocks_;
  // A map from the pointer value ID from the trace to the actual pointer value
  // allocated by the allocator.
  IdMap id_map_;

  util::Rng rng_;

  bool verbose_;
};

}  // namespace bench

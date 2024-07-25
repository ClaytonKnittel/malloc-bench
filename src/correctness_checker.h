#pragma once

#include <cstddef>
#include <optional>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"

#include "src/rng.h"
#include "src/tracefile_reader.h"

namespace bench {

class CorrectnessChecker {
 public:
  static absl::Status Check(const std::string& tracefile);

 private:
  struct AllocatedBlock {
    size_t size;
    uint64_t magic_bytes;
  };

  using Map = absl::btree_map<void*, AllocatedBlock>;
  using IdMap = absl::flat_hash_map<void*, void*>;

  explicit CorrectnessChecker(TracefileReader&& reader);

  absl::Status Run();

  absl::Status Malloc(size_t size, void* id, bool is_calloc);
  absl::Status Realloc(void* orig_id, size_t size, void* new_id);
  absl::Status Free(void* id);

  std::optional<Map::const_iterator> FindContainingBlock(void* ptr) const;

  TracefileReader reader_;

  Map allocated_blocks_;
  // A map from the pointer value ID from the trace to the actual pointer value
  // allocated by the allocator.
  IdMap id_map_;

  util::Rng rng_;
};

}  // namespace bench

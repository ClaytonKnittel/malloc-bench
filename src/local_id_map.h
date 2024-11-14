#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"

#include "src/concurrent_id_map.h"
#include "src/tracefile_reader.h"

namespace bench {

class LocalIdMap {
 public:
  static constexpr size_t kBatchSize = 512;
  static constexpr size_t kMaxQueuedOpsTaken = 128;

  LocalIdMap(std::atomic<uint64_t>& idx, const Tracefile& tracefile,
             ConcurrentIdMap& global_id_map, uint64_t num_repetitions);

  absl::Status FlushOps(size_t num_ops,
                        std::pair<const TraceLine*, uint64_t>* ops);

 private:
  static std::pair<std::optional<uint64_t>, std::optional<uint64_t>>
  InputAndResultIds(const TraceLine& line);

  static void SetInputId(TraceLine& line, uint64_t input_id);

  static void SetResultId(TraceLine& line, uint64_t result_id);

  absl::StatusOr<size_t> PrepareBatch();

  size_t PrepareOpsFromTrace(size_t num_trace_ops_to_take,
                             std::pair<const TraceLine*, uint64_t>* ops);

  // Checks if an operation will be possible, given the set of local
  // allocations (i.e. allocations made by this thread so far since the last
  // sync) and already-committed global allocations. If this returns false,
  // then `line` is placed in the global queue and can be skipped for now.
  bool CanDoOpOrQueue(absl::flat_hash_set<uint64_t>& local_allocations,
                      const TraceLine& line, uint64_t iteration);

  // If this operation freed any memory, this will erase the associated
  // metadata in the global id map.
  absl::Status EraseFreedAlloc(const TraceLine& line, uint64_t iteration);

  std::atomic<uint64_t>& idx_;
  const Tracefile& tracefile_;
  const uint64_t num_repetitions_;
  ConcurrentIdMap& global_id_map_;
  void* id_map_[kBatchSize];
};

}  // namespace bench

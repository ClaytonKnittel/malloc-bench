#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>

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

  // A batch context contains a sequence of modified trace line operations whose
  // indices correspond to indices in a local array of allocated pointers.
  //
  // The local array of allocated pointers is populated with already-allocated
  // pointers from the global ID map which will be freed by allocations made in
  // this batch. Trace lines which allocate new memory will reserve unused slots
  // in this local array.
  //
  // After all operations in `Ops()` have been performed by the allocator,
  // `AllocationsToRecord()` will yield all of the allocated pointers which need
  // to be added to the global ID map.
  class BatchContext {
   public:
    // Given a list of { trace line, iteration } pairs to execute, constructs a
    // batch context.
    static absl::StatusOr<BatchContext> MakeFromOps(
        size_t num_ops, const std::pair<const TraceLine*, uint64_t>* ops,
        ConcurrentIdMap& global_id_map, const proto::Tracefile& tracefile);

    uint64_t NumOps() const {
      return num_ops_;
    }

    auto Ops() const {
      return ops_ | std::views::take(num_ops_);
    }

    // Returns a range over pairs `{unique_id, allocated pointer}` of
    // allocations which are not freed after this batch is complete. Should only
    // be called after all operations in the batch have been executed.
    auto AllocationsToRecord() const {
      return id_to_idx_ | std::views::transform([this](const auto& id_idx) {
               auto [unique_id, idx] = id_idx;
               return std::make_pair(unique_id, id_map_[idx]);
             });
    }

    std::array<void*, kBatchSize>& IdMap() {
      return id_map_;
    }

   private:
    explicit BatchContext(uint64_t num_ops) : num_ops_(num_ops) {}

    uint64_t num_ops_;
    std::array<TraceLine, kBatchSize> ops_;

    // A map from unique id's of an operation to the index of the result of the
    // operation in id_map_. This only contains allocations which are not freed
    // in this batch (and must be flushed to the global ID map).
    absl::flat_hash_map<uint64_t, size_t> id_to_idx_;

    std::array<void*, kBatchSize> id_map_;
  };

  LocalIdMap(std::atomic<uint64_t>& idx, const Tracefile& tracefile,
             ConcurrentIdMap& global_id_map, uint64_t num_repetitions);

  absl::StatusOr<BatchContext> PrepareBatch();

  absl::Status FlushOps(const BatchContext& context);

 private:
  static std::pair<std::optional<uint64_t>, std::optional<uint64_t>>
  InputAndResultIds(const TraceLine& line);

  static void SetInputId(TraceLine& line, uint64_t input_id);

  static void SetResultId(TraceLine& line, uint64_t result_id);

  size_t PrepareOpsFromTrace(size_t num_trace_ops_to_take,
                             std::pair<const TraceLine*, uint64_t>* ops);

  // Checks if an operation will be possible, given the set of local
  // allocations (i.e. allocations made by this thread so far since the last
  // sync) and already-committed global allocations. If this returns false,
  // then `line` is placed in the global queue and can be skipped for now.
  bool CanDoOpOrQueue(absl::flat_hash_set<uint64_t>& local_allocations,
                      const TraceLine& line, uint64_t iteration);

  std::atomic<uint64_t>& idx_;
  const Tracefile& tracefile_;
  const uint64_t num_repetitions_;
  ConcurrentIdMap& global_id_map_;
};

}  // namespace bench

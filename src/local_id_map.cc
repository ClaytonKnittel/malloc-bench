#include "src/local_id_map.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "absl/container/btree_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "util/absl_util.h"

#include "proto/tracefile.pb.h"
#include "src/concurrent_id_map.h"
#include "src/perfetto.h"  // IWYU pragma: keep
#include "src/tracefile_reader.h"

namespace bench {

namespace {

class UniqueTemporalIdGenerator {
 public:
  uint64_t NextUnusedId() {
    return next_id_++;
  }

  uint64_t NextId() {
    auto it = available_ids_.begin();
    if (it != available_ids_.end()) {
      uint64_t next_id = *it;
      available_ids_.erase(it);
      return next_id;
    }

    return NextUnusedId();
  }

  void FreeId(uint64_t id) {
    available_ids_.insert(id);
  }

 private:
  uint64_t next_id_ = 0;
  absl::btree_set<uint64_t> available_ids_;
};

}  // namespace

/* static */
absl::StatusOr<LocalIdMap::BatchContext> LocalIdMap::BatchContext::MakeFromOps(
    size_t num_ops, const std::pair<const TraceLine*, uint64_t>* ops,
    ConcurrentIdMap& global_id_map, const proto::Tracefile& tracefile) {
  BatchContext context(num_ops);

  UniqueTemporalIdGenerator id_gen;
  for (size_t i = 0; i < num_ops; i++) {
    const auto& [line_ptr, iteration] = ops[i];
    TraceLine line = *line_ptr;
    auto [input_id, result_id] = InputAndResultIds(line);

    if (input_id.has_value()) {
      uint64_t unique_id =
          ConcurrentIdMap::UniqueId(input_id.value(), iteration, tracefile);
      auto it = context.id_to_idx_.find(unique_id);
      uint64_t idx;
      if (it != context.id_to_idx_.end()) {
        idx = it->second;
        context.id_to_idx_.erase(it);
      } else {
        idx = id_gen.NextUnusedId();

        auto allocation = global_id_map.LookupAllocation(unique_id);
        if (!allocation.has_value()) {
          return absl::InternalError(absl::StrFormat(
              "No allocation found with unique id %v", unique_id));
        }
        // Since we will be performing this allocation which will be freeing the
        // memory associated with `input_id`, we can erase the mapping in the
        // global ID map.
        RETURN_IF_ERROR(global_id_map.AddFree(unique_id));

        context.id_map_[idx] = allocation.value();
      }

      SetInputId(line, idx);
      id_gen.FreeId(idx);
    }
    if (result_id.has_value()) {
      uint64_t unique_id =
          ConcurrentIdMap::UniqueId(result_id.value(), iteration, tracefile);
      uint64_t idx = id_gen.NextId();
      auto [it, inserted] = context.id_to_idx_.insert({ unique_id, idx });
      if (!inserted) {
        return absl::InternalError(
            absl::StrFormat("Duplicate unique ID encountered while "
                            "preparing allocation batch: %v",
                            unique_id));
      }

      SetResultId(line, idx);
    }

    context.ops_[i] = std::move(line);
  }

  return context;
}

LocalIdMap::LocalIdMap(std::atomic<uint64_t>& idx, const Tracefile& tracefile,
                       ConcurrentIdMap& global_id_map, uint64_t num_repetitions)
    : idx_(idx),
      tracefile_(tracefile),
      num_repetitions_(num_repetitions),
      global_id_map_(global_id_map) {}

absl::StatusOr<LocalIdMap::BatchContext> LocalIdMap::PrepareBatch() {
  TRACE_EVENT("test_infrastructure", "LocalIdMap::PrepareBatch");

  std::pair<const TraceLine*, uint64_t> ops[kBatchSize];
  const size_t queued_ops_taken =
      global_id_map_.TakeFromQueue(ops, kMaxQueuedOpsTaken);
  const size_t trace_ops_taken = PrepareOpsFromTrace(
      kBatchSize - queued_ops_taken, &ops[queued_ops_taken]);
  const size_t total_ops = queued_ops_taken + trace_ops_taken;

  return BatchContext::MakeFromOps(total_ops, ops, global_id_map_, tracefile_);
}

absl::Status LocalIdMap::FlushOps(const BatchContext& context) {
  TRACE_EVENT("test_infrastructure", "LocalIdMap::FlushOps");
  for (const auto [id, allocated_ptr] : context.AllocationsToRecord()) {
    RETURN_IF_ERROR(global_id_map_.AddAllocation(id, allocated_ptr));
  }
  return absl::OkStatus();
}

/* static */
std::pair<std::optional<uint64_t>, std::optional<uint64_t>>
LocalIdMap::InputAndResultIds(const TraceLine& line) {
  std::optional<uint64_t> input_id;
  std::optional<uint64_t> result_id;
  switch (line.op_case()) {
    case TraceLine::kMalloc: {
      const TraceLine::Malloc& malloc = line.malloc();
      if (malloc.has_result_id()) {
        result_id = malloc.result_id();
      }
      break;
    }
    case TraceLine::kCalloc: {
      const TraceLine::Calloc& calloc = line.calloc();
      if (calloc.has_result_id()) {
        result_id = calloc.result_id();
      }
      break;
    }
    case TraceLine::kRealloc: {
      const TraceLine::Realloc& realloc = line.realloc();
      if (realloc.has_input_id()) {
        input_id = realloc.input_id();
      }
      result_id = realloc.result_id();
      break;
    }
    case TraceLine::kFree: {
      const TraceLine::Free& free = line.free();
      if (free.has_input_id()) {
        input_id = free.input_id();
      }
      break;
    }
    case TraceLine::OP_NOT_SET: {
      break;
    }
  }

  return { input_id, result_id };
}

/* static */
void LocalIdMap::SetInputId(TraceLine& line, uint64_t input_id) {
  switch (line.op_case()) {
    case TraceLine::kRealloc: {
      line.mutable_realloc()->set_input_id(input_id);
      break;
    }
    case TraceLine::kFree: {
      line.mutable_free()->set_input_id(input_id);
      break;
    }
    case TraceLine::kMalloc:
    case TraceLine::kCalloc:
    case TraceLine::OP_NOT_SET: {
      break;
    }
  }
}

/* static */
void LocalIdMap::SetResultId(TraceLine& line, uint64_t result_id) {
  switch (line.op_case()) {
    case TraceLine::kMalloc: {
      line.mutable_malloc()->set_result_id(result_id);
      break;
    }
    case TraceLine::kCalloc: {
      line.mutable_calloc()->set_result_id(result_id);
      break;
    }
    case TraceLine::kRealloc: {
      line.mutable_realloc()->set_result_id(result_id);
      break;
    }
    case TraceLine::kFree:
    case TraceLine::OP_NOT_SET: {
      break;
    }
  }
}

size_t LocalIdMap::PrepareOpsFromTrace(
    size_t num_trace_ops_to_take, std::pair<const TraceLine*, uint64_t>* ops) {
  TRACE_EVENT("test_infrastructure", "LocalIdMap::PrepareOpsFromTrace");
  size_t trace_ops_taken = 0;
  absl::flat_hash_set<uint64_t> local_allocations;

  while (trace_ops_taken < num_trace_ops_to_take) {
    const size_t remaining_ops_to_take =
        num_trace_ops_to_take - trace_ops_taken;
    size_t first_idx =
        idx_.fetch_add(remaining_ops_to_take, std::memory_order_relaxed);
    if (first_idx >= num_repetitions_ * tracefile_.lines_size()) {
      idx_.store(num_repetitions_ * tracefile_.lines_size(),
                 std::memory_order_relaxed);
      first_idx = num_repetitions_ * tracefile_.lines_size();
    }

    const size_t end_idx =
        std::min<size_t>(first_idx + remaining_ops_to_take,
                         num_repetitions_ * tracefile_.lines_size());
    if (first_idx == end_idx) {
      break;
    }

    for (size_t i = first_idx; i < end_idx; i++) {
      size_t line_idx = i % tracefile_.lines_size();
      uint64_t iteration = i / tracefile_.lines_size();

      if (!CanDoOpOrQueue(local_allocations, tracefile_.lines(line_idx),
                          iteration)) {
        continue;
      }

      ops[trace_ops_taken] =
          std::make_pair(&tracefile_.lines(line_idx), iteration);
      trace_ops_taken++;
    }
  }

  return trace_ops_taken;
}

bool LocalIdMap::CanDoOpOrQueue(
    absl::flat_hash_set<uint64_t>& local_allocations, const TraceLine& line,
    uint64_t iteration) {
  auto [input_id, result_id] = InputAndResultIds(line);

  if (input_id.has_value()) {
    uint64_t id =
        ConcurrentIdMap::UniqueId(input_id.value(), iteration, tracefile_);
    auto local_it = local_allocations.find(id);
    if (local_it != local_allocations.end()) {
      local_allocations.erase(local_it);
    } else if (global_id_map_.MaybeSuspendAllocation(id,
                                                     { &line, iteration })) {
      // If the allocation was suspended, then its dependent operation has
      // not been completed yet. We shouldn't try to perform this op, so
      // skip it.
      return false;
    }
  }
  if (result_id.has_value()) {
    local_allocations.insert(
        ConcurrentIdMap::UniqueId(result_id.value(), iteration, tracefile_));
  }
  return true;
}

}  // namespace bench

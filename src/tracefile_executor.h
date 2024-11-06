#pragma once

#include <atomic>
#include <barrier>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <thread>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "folly/concurrency/ConcurrentHashMap.h"
#include "util/absl_util.h"

#include "proto/tracefile.pb.h"
#include "src/tracefile_reader.h"
#include "src/util.h"

namespace bench {

using proto::Tracefile;
using proto::TraceLine;

template <typename T>
concept IdMapContainer = requires(T t, uint64_t id, void* ptr,
                                  std::pair<const TraceLine*, uint64_t> idx) {
  { t.SetId(id, ptr) } -> std::convertible_to<bool>;
  { t.GetOrQueueId(id, idx) } -> std::convertible_to<std::optional<void*>>;
  { t.ClearId(id) } -> std::convertible_to<size_t>;
};

template <typename T>
concept TracefileAllocator = requires(T allocator, size_t size, void* ptr,
                                      std::optional<size_t> alignment) {
  { allocator.InitializeHeap() } -> std::convertible_to<absl::Status>;
  { allocator.CleanupHeap() } -> std::convertible_to<absl::Status>;
  {
    allocator.Malloc(size, alignment)
  } -> std::convertible_to<absl::StatusOr<void*>>;
  {
    allocator.Calloc(size, size)
  } -> std::convertible_to<absl::StatusOr<void*>>;
  {
    allocator.Realloc(ptr, size)
  } -> std::convertible_to<absl::StatusOr<void*>>;
  {
    allocator.Free(ptr, alignment, alignment)
  } -> std::convertible_to<absl::Status>;
};

struct TracefileExecutorOptions {
  uint32_t n_threads = 1;
};

template <TracefileAllocator Allocator>
class TracefileExecutor {
 public:
  template <typename... Args>
  explicit TracefileExecutor(TracefileReader& reader, Args... args);

  // hi i am a coder woww i am going to hack into your compouter now with mty
  // computer skills hohohohoho
  absl::StatusOr<absl::Duration> Run(
      const TracefileExecutorOptions& options = TracefileExecutorOptions());

  absl::StatusOr<absl::Duration> RunRepeated(
      uint64_t num_repetitions,
      const TracefileExecutorOptions& options = TracefileExecutorOptions());

  Allocator& Inner() {
    return allocator_;
  }
  const Allocator& Inner() const {
    return allocator_;
  }

 private:
  class ConcurrentIdMap {
   public:
    // Adds an allocation to the map. Returns a failure status if it failed
    // because the key `id` was already in use.
    absl::Status AddAllocation(uint64_t id, void* allocated_ptr) {
      auto [it, inserted] =
          id_map_.insert(id, MapVal{ .allocated_ptr = allocated_ptr });
      if (!inserted) {
        // If the insertion failed, that means there was a pending allocation
        // marker in this slot.
        auto pending_idx = it->second.idx;

        // Replace the slot with the allocated pointer...
        auto result =
            id_map_.assign(id, MapVal{ .allocated_ptr = allocated_ptr });
        if (!result.has_value()) {
          return absl::InternalError(absl::StrFormat(
              "Failed to insert %v after queuing pending operation.", id));
        }

        // and then push the pending index to the queue.
        {
          absl::MutexLock lock(&queue_lock_);
          queued_ops_.push_back(std::move(pending_idx));
        }
      }

      return absl::OkStatus();
    }

    // Removes a tracked allocation from the map (because it was freed). Returns
    // an error status if the removal failed because the key `id` was not found.
    absl::Status AddFree(uint64_t id) {
      size_t erased_elems = id_map_.erase(id);
      if (erased_elems != 1) {
        return absl::InternalError(absl::StrFormat(
            "Failed to erase ID %v from the map, not found", id));
      }
      return absl::OkStatus();
    }

    // Looks up an allocation by ID, returning the pointer allocated with this
    // ID if it exists, otherwise `std::nullopt`.
    std::optional<void*> LookupAllocation(uint64_t id) {
      auto it = id_map_.find(id);
      if (it == id_map_.end()) {
        return std::nullopt;
      }
      return it->second.allocated_ptr;
    }

    // Queues an allocation that was previously not able to execute. This will
    // atomically check for an allocation made under `id`, and if not found will
    // insert `idx` into the id map as a dependent operation. If an allocation
    // was found to be made, this operation is instead directly inserted into
    // the queue.
    void QueueAllocation(uint64_t id,
                         std::pair<const TraceLine*, uint64_t> idx) {
      auto [it, inserted] = id_map_.insert(id, MapVal{ .idx = idx });
      if (!inserted) {
        absl::MutexLock lock(&queue_lock_);
        queued_ops_.push_back(std::move(idx));
      }
    }

    size_t TakeFromQueue(std::pair<const TraceLine*, uint64_t> (&array)[],
                         size_t array_len) {
      absl::MutexLock lock(&queue_lock_);
      uint32_t n_taken_elements =
          static_cast<uint32_t>(std::min(queued_ops_.size(), array_len));
      for (uint32_t i = 0; i < n_taken_elements; i++) {
        array[i] = queued_ops_.front();
        queued_ops_.pop_front();
      }
      return n_taken_elements;
    }

   private:
    union MapVal {
      void* allocated_ptr;
      std::pair<const TraceLine*, uint64_t> idx;
    };

    folly::ConcurrentHashMap<uint64_t, MapVal> id_map_;
    absl::Mutex queue_lock_;
    std::deque<std::pair<const TraceLine*, uint64_t>> queued_ops_
        BENCH_GUARDED_BY(queue_lock_);
  };

  uint64_t UniqueId(uint64_t id, uint64_t iteration) {
    return id + iteration * reader_.size();
  }

  template <IdMapContainer IdMap>
  absl::Status DoMalloc(const TraceLine& line, uint64_t iteration,
                        IdMap& id_map);

  template <IdMapContainer IdMap>
  absl::Status DoCalloc(const TraceLine& line, uint64_t iteration,
                        IdMap& id_map);

  template <IdMapContainer IdMap>
  absl::Status DoRealloc(const TraceLine& line, uint64_t iteration,
                         IdMap& id_map);

  template <IdMapContainer IdMap>
  absl::Status DoFree(const TraceLine& line, uint64_t iteration, IdMap& id_map);

  absl::StatusOr<absl::Duration> ProcessTracefile(uint64_t num_repetitions);

  absl::StatusOr<absl::Duration> ProcessTracefileMultithreaded(
      uint64_t num_repetitions, const TracefileExecutorOptions& options);

  // Worker thread main loop, returns the total amount of time spend in
  // allocation code (filtering out *most* of the expensive testing
  // infrastructure logic).
  absl::StatusOr<absl::Duration> ProcessorWorker(std::barrier<>& barrier,
                                                 std::atomic<size_t>& idx,
                                                 std::atomic<bool>& done,
                                                 const Tracefile& tracefile,
                                                 ConcurrentIdMap& global_id_map,
                                                 uint64_t num_repetitions);

  static absl::Status RewriteIdsToUnique(Tracefile& tracefile);

  template <IdMapContainer IdMap>
  absl::Status ProcessLine(const TraceLine& line, uint64_t iteration,
                           IdMap& id_map);

  Allocator allocator_;

  TracefileReader& reader_;
};

template <TracefileAllocator Allocator>
template <typename... Args>
TracefileExecutor<Allocator>::TracefileExecutor(TracefileReader& reader,
                                                Args... args)
    : allocator_(std::forward<Args>(args)...), reader_(reader) {}

template <TracefileAllocator Allocator>
absl::StatusOr<absl::Duration> TracefileExecutor<Allocator>::Run(
    const TracefileExecutorOptions& options) {
  return RunRepeated(/*num_repetitions=*/1, options);
}

template <TracefileAllocator Allocator>
absl::StatusOr<absl::Duration> TracefileExecutor<Allocator>::RunRepeated(
    uint64_t num_repetitions, const TracefileExecutorOptions& options) {
  RETURN_IF_ERROR(allocator_.InitializeHeap());

  absl::StatusOr<absl::Duration> result;
  if (options.n_threads == 1) {
    result = ProcessTracefile(num_repetitions);
  } else {
    result = ProcessTracefileMultithreaded(num_repetitions, options);
  }

  RETURN_IF_ERROR(allocator_.CleanupHeap());
  return result;
}

template <TracefileAllocator Allocator>
template <IdMapContainer IdMap>
absl::Status TracefileExecutor<Allocator>::DoMalloc(const TraceLine& line,
                                                    uint64_t iteration,
                                                    IdMap& id_map) {
  const TraceLine::Malloc& malloc = line.malloc();
  std::optional<size_t> alignment =
      malloc.has_input_alignment() ? std::optional(malloc.input_alignment())
                                   : std::nullopt;
  DEFINE_OR_RETURN(void*, ptr,
                   allocator_.Malloc(malloc.input_size(), alignment));

  if (malloc.input_size() != 0 && malloc.has_result_id()) {
    uint64_t unique_id = UniqueId(malloc.result_id(), iteration);
    if (!id_map.SetId(unique_id, ptr)) {
      return absl::InternalError(absl::StrFormat(
          "Failed to set ID %v (%v) for malloc in id map, already exists",
          unique_id, malloc.result_id()));
    }
  }

  return absl::OkStatus();
}

template <TracefileAllocator Allocator>
template <IdMapContainer IdMap>
absl::Status TracefileExecutor<Allocator>::DoCalloc(const TraceLine& line,
                                                    uint64_t iteration,
                                                    IdMap& id_map) {
  const TraceLine::Calloc& calloc = line.calloc();
  DEFINE_OR_RETURN(
      void*, ptr, allocator_.Calloc(calloc.input_nmemb(), calloc.input_size()));

  if (calloc.input_nmemb() != 0 && calloc.input_size() != 0 &&
      calloc.has_result_id()) {
    uint64_t unique_id = UniqueId(calloc.result_id(), iteration);
    if (!id_map.SetId(unique_id, ptr)) {
      return absl::InternalError(absl::StrFormat(
          "Failed to set ID %v (%v) for calloc in id map, already exists",
          unique_id, calloc.result_id()));
    }
  }

  return absl::OkStatus();
}

template <TracefileAllocator Allocator>
template <IdMapContainer IdMap>
absl::Status TracefileExecutor<Allocator>::DoRealloc(const TraceLine& line,
                                                     uint64_t iteration,
                                                     IdMap& id_map) {
  const TraceLine::Realloc& realloc = line.realloc();
  void* input_ptr;
  if (realloc.has_input_id()) {
    uint64_t unique_id = UniqueId(realloc.input_id(), iteration);
    std::optional<void*> result =
        id_map.GetOrQueueId(unique_id, { &line, iteration });
    if (!result.has_value()) {
      return absl::OkStatus();
    }
    input_ptr = result.value();

    if (id_map.ClearId(unique_id) != 1) {
      return absl::InternalError(absl::StrFormat(
          "ID %v (%v) for realloc erased by other concurrent op", unique_id,
          realloc.input_id()));
    }
  } else {
    input_ptr = nullptr;
  }
  DEFINE_OR_RETURN(void*, result_ptr,
                   allocator_.Realloc(input_ptr, realloc.input_size()));
  uint64_t unique_id = UniqueId(realloc.result_id(), iteration);
  if (!id_map.SetId(unique_id, result_ptr)) {
    return absl::InternalError(absl::StrFormat(
        "Failed to set ID %v (%v) for realloc in id map, already exists",
        unique_id, realloc.result_id()));
  }

  return absl::OkStatus();
}

template <TracefileAllocator Allocator>
template <IdMapContainer IdMap>
absl::Status TracefileExecutor<Allocator>::DoFree(const TraceLine& line,
                                                  uint64_t iteration,
                                                  IdMap& id_map) {
  const TraceLine::Free& free = line.free();
  if (!free.has_input_id()) {
    return allocator_.Free(nullptr, std::nullopt, std::nullopt);
  }

  uint64_t unique_id = UniqueId(free.input_id(), iteration);
  std::optional<void*> result =
      id_map.GetOrQueueId(unique_id, { &line, iteration });
  if (!result.has_value()) {
    return absl::OkStatus();
  }
  void* ptr = result.value();

  std::optional<size_t> size_hint = free.has_input_size_hint()
                                        ? std::optional(free.input_size_hint())
                                        : std::nullopt;
  std::optional<size_t> alignment_hint =
      free.has_input_alignment_hint()
          ? std::optional(free.input_alignment_hint())
          : std::nullopt;
  RETURN_IF_ERROR(allocator_.Free(ptr, size_hint, alignment_hint));

  if (id_map.ClearId(unique_id) != 1) {
    return absl::InternalError(
        absl::StrFormat("ID %v (%v) for free erased by other concurrent op",
                        unique_id, free.input_id()));
  }

  return absl::OkStatus();
}

template <TracefileAllocator Allocator>
absl::StatusOr<absl::Duration> TracefileExecutor<Allocator>::ProcessTracefile(
    uint64_t num_repetitions) {
  // A map from allocation id's to pointers returned from the allocator. Since
  // id's are assigned contiguously from lowest to highest ID, they can be
  // stored in a vector.
  struct VectorIdMap {
    std::vector<void*> id_map;

    bool SetId(uint64_t id, void* ptr) {
      id_map[id] = ptr;
      return true;
    }
    std::optional<void*> GetOrQueueId(
        uint64_t id, std::pair<const TraceLine*, uint64_t> idx) const {
      (void) idx;
      return id_map[id];
    }
    static size_t ClearId(uint64_t id) {
      (void) id;
      return 1;
    }
  };

  size_t max_simultaneous_allocs =
      reader_.Tracefile().max_simultaneous_allocs();
  VectorIdMap id_map{ .id_map = std::vector<void*>(max_simultaneous_allocs) };

  absl::Time start = absl::Now();
  for (uint64_t t = 0; t < num_repetitions; t++) {
    for (const TraceLine& line : reader_) {
      switch (line.op_case()) {
          // Note: iteration should be kept 0 here. It is used to distinguish
          // repeated operations in multithreaded mode when repeating a trace,
          // but this problem does not exist for single-threaded mode. Since
          // indices here are into a vector, and not a hash map, they must be
          // contained in the expected range.

        case TraceLine::kMalloc: {
          RETURN_IF_ERROR(DoMalloc(line, /*iteration=*/0, id_map));
          break;
        }
        case TraceLine::kCalloc: {
          RETURN_IF_ERROR(DoCalloc(line, /*iteration=*/0, id_map));
          break;
        }
        case TraceLine::kRealloc: {
          RETURN_IF_ERROR(DoRealloc(line, /*iteration=*/0, id_map));
          break;
        }
        case TraceLine::kFree: {
          RETURN_IF_ERROR(DoFree(line, /*iteration=*/0, id_map));
          break;
        }
        case TraceLine::OP_NOT_SET: {
          return absl::FailedPreconditionError("Op not set in tracefile");
        }
      }
    }
  }
  absl::Time end = absl::Now();

  return end - start;
}

template <TracefileAllocator Allocator>
absl::StatusOr<absl::Duration>
TracefileExecutor<Allocator>::ProcessTracefileMultithreaded(
    uint64_t num_repetitions, const TracefileExecutorOptions& options) {
  Tracefile tracefile(reader_.Tracefile());
  RETURN_IF_ERROR(RewriteIdsToUnique(tracefile));

  absl::Duration max_allocation_time;
  absl::Status status = absl::OkStatus();
  absl::Mutex status_lock;

  {
    std::barrier barrier(options.n_threads);

    std::atomic<bool> done = false;

    std::atomic<size_t> idx = 0;
    ConcurrentIdMap global_id_map;

    std::vector<std::thread> threads;
    threads.reserve(options.n_threads);

    for (uint32_t i = 0; i < options.n_threads; i++) {
      threads.emplace_back([this, &max_allocation_time, &status, &status_lock,
                            &barrier, &done, &idx, &tracefile, &global_id_map,
                            num_repetitions]() {
        auto result = ProcessorWorker(barrier, idx, done, tracefile,
                                      global_id_map, num_repetitions);
        barrier.arrive_and_drop();

        if (result.ok()) {
          absl::MutexLock lock(&status_lock);
          max_allocation_time = std::max(result.value(), max_allocation_time);
        } else {
          done.store(true, std::memory_order_relaxed);

          absl::MutexLock lock(&status_lock);
          if (status.ok()) {
            status = result.status();
          }
        }
      });
    }

    for (uint32_t i = 0; i < options.n_threads; i++) {
      threads[i].join();
    }

    if (!status.ok()) {
      return status;
    }
  }

  return max_allocation_time;
}

template <TracefileAllocator Allocator>
absl::StatusOr<absl::Duration> TracefileExecutor<Allocator>::ProcessorWorker(
    std::barrier<>& barrier, std::atomic<size_t>& idx, std::atomic<bool>& done,
    const Tracefile& tracefile, ConcurrentIdMap& global_id_map,
    uint64_t num_repetitions) {
  static constexpr size_t kBatchSize = 512;
  static constexpr size_t kMaxQueuedOpsTaken = 128;

  class LocalIdMap {
   public:
    explicit LocalIdMap(ConcurrentIdMap& global_id_map)
        : global_id_map_(global_id_map) {
      id_map_.reserve(kBatchSize);
      erased_ids_.reserve(kBatchSize);
      pending_ops_.reserve(kBatchSize);
    }

    bool SetId(uint64_t id, void* ptr) {
      auto [it, inserted] = id_map_.insert({ id, ptr });
      return inserted;
    }

    std::optional<void*> GetOrQueueId(
        uint64_t id, std::pair<const TraceLine*, uint64_t> idx) {
      auto it = id_map_.find(id);
      if (it != id_map_.end()) {
        return it->second;
      }

      auto result = global_id_map_.LookupAllocation(id);
      if (result.has_value()) {
        return result.value();
      }

      pending_ops_.emplace_back(id, std::move(idx));
      return std::nullopt;
    }

    size_t ClearId(uint64_t id) {
      if (id_map_.erase(id) == 0) {
        erased_ids_.push_back(id);
      }
      return 1;
    }

    absl::Status FlushOps() {
      for (const auto [id, ptr] : id_map_) {
        RETURN_IF_ERROR(global_id_map_.AddAllocation(id, ptr));
      }
      for (uint64_t erased_id : erased_ids_) {
        RETURN_IF_ERROR(global_id_map_.AddFree(erased_id));
      }
      for (auto [id, idx] : pending_ops_) {
        global_id_map_.QueueAllocation(id, std::move(idx));
      }
      id_map_.clear();
      erased_ids_.clear();
      pending_ops_.clear();
      return absl::OkStatus();
    }

   private:
    ConcurrentIdMap& global_id_map_;
    absl::flat_hash_map<uint64_t, void*> id_map_;
    std::vector<uint64_t> erased_ids_;
    std::vector<std::pair<uint64_t, std::pair<const TraceLine*, uint64_t>>>
        pending_ops_;
  };

  absl::Duration time;

  LocalIdMap local_id_map(global_id_map);
  while (!done.load(std::memory_order_relaxed)) {
    std::pair<const TraceLine*, uint64_t> ops[kBatchSize];
    const size_t queued_ops_taken =
        global_id_map.TakeFromQueue(ops, kMaxQueuedOpsTaken);

    const size_t num_trace_ops_to_take = kBatchSize - queued_ops_taken;
    size_t first_idx =
        idx.fetch_add(num_trace_ops_to_take, std::memory_order_relaxed);
    if (first_idx >= num_repetitions * tracefile.lines_size()) {
      idx.store(num_repetitions * tracefile.lines_size(),
                std::memory_order_relaxed);
      first_idx = num_repetitions * tracefile.lines_size();
    }

    const size_t end_idx =
        std::min<size_t>(first_idx + num_trace_ops_to_take,
                         num_repetitions * tracefile.lines_size());
    const size_t trace_ops_taken = end_idx - first_idx;
    for (size_t i = first_idx; i < end_idx; i++) {
      size_t line_idx = i % tracefile.lines_size();
      uint64_t iteration = i / tracefile.lines_size();
      ops[queued_ops_taken + (i - first_idx)] =
          std::make_pair(&tracefile.lines(line_idx), iteration);
    }

    const size_t total_ops_taken = queued_ops_taken + trace_ops_taken;
    if (total_ops_taken == 0) {
      break;
    }

    barrier.arrive_and_wait();

    absl::Time start = absl::Now();
    for (size_t i = 0; i < total_ops_taken; i++) {
      auto [line, iteration] = ops[i];
      RETURN_IF_ERROR(ProcessLine(*line, iteration, local_id_map));
    }
    absl::Time end = absl::Now();
    time += end - start;

    RETURN_IF_ERROR(local_id_map.FlushOps());
  }

  return time;
}

/* static */
template <TracefileAllocator Allocator>
absl::Status TracefileExecutor<Allocator>::RewriteIdsToUnique(
    Tracefile& tracefile) {
  uint64_t next_id = 0;
  absl::flat_hash_map<uint64_t, std::pair<uint64_t, size_t>> new_id_map;

  for (TraceLine& line : *tracefile.mutable_lines()) {
    switch (line.op_case()) {
      case TraceLine::kMalloc: {
        TraceLine::Malloc& malloc = *line.mutable_malloc();
        if (!malloc.has_result_id()) {
          break;
        }

        auto [it, inserted] = new_id_map.insert(
            { malloc.result_id(), { next_id, malloc.input_size() } });
        if (!inserted) {
          return absl::FailedPreconditionError(
              absl::StrFormat("Duplicate result ID %v", malloc.result_id()));
        }
        malloc.set_result_id(next_id);
        next_id++;
        break;
      }
      case TraceLine::kCalloc: {
        TraceLine::Calloc& calloc = *line.mutable_calloc();
        if (!calloc.has_result_id()) {
          break;
        }

        auto [it, inserted] = new_id_map.insert(
            { calloc.result_id(),
              { next_id, calloc.input_nmemb() * calloc.input_size() } });
        if (!inserted) {
          return absl::FailedPreconditionError(
              absl::StrFormat("Duplicate result ID %v", calloc.result_id()));
        }
        calloc.set_result_id(next_id);
        next_id++;
        break;
      }
      case TraceLine::kRealloc: {
        TraceLine::Realloc& realloc = *line.mutable_realloc();
        if (realloc.has_input_id()) {
          auto release_it = new_id_map.find(realloc.input_id());
          if (release_it == new_id_map.end()) {
            return absl::FailedPreconditionError(absl::StrFormat(
                "Unknown ID being realloc-ed: %v", realloc.input_id()));
          }
          realloc.set_input_id(release_it->second.first);
          new_id_map.erase(release_it);
        }

        auto [it, inserted] = new_id_map.insert(
            { realloc.result_id(), { next_id, realloc.input_size() } });
        if (!inserted) {
          return absl::FailedPreconditionError(
              absl::StrFormat("Duplicate result ID %v", realloc.result_id()));
        }
        realloc.set_result_id(next_id);
        next_id++;
        break;
      }
      case TraceLine::kFree: {
        TraceLine::Free& free = *line.mutable_free();
        if (!free.has_input_id()) {
          break;
        }

        auto release_it = new_id_map.find(free.input_id());
        if (release_it == new_id_map.end()) {
          return absl::FailedPreconditionError(
              absl::StrFormat("Unknown ID being freed: %v", free.input_id()));
        }
        free.set_input_id(release_it->second.first);
        new_id_map.erase(release_it);
        break;
      }
      case TraceLine::OP_NOT_SET: {
        return absl::FailedPreconditionError("Op not set in tracefile");
      }
    }
  }

  if (!new_id_map.empty()) {
    return absl::FailedPreconditionError(
        "Not all allocations freed in tracefile");
  }

  return absl::OkStatus();
}

template <TracefileAllocator Allocator>
template <IdMapContainer IdMap>
absl::Status TracefileExecutor<Allocator>::ProcessLine(const TraceLine& line,
                                                       uint64_t iteration,
                                                       IdMap& id_map) {
  switch (line.op_case()) {
    case TraceLine::kMalloc: {
      return DoMalloc(line, iteration, id_map);
    }
    case TraceLine::kCalloc: {
      return DoCalloc(line, iteration, id_map);
    }
    case TraceLine::kRealloc: {
      return DoRealloc(line, iteration, id_map);
    }
    case TraceLine::kFree: {
      return DoFree(line, iteration, id_map);
    }
    case TraceLine::OP_NOT_SET: {
      __builtin_unreachable();
    }
  }
}

}  // namespace bench

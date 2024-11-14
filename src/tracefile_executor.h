#pragma once

#include <atomic>
#include <barrier>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <thread>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "folly/concurrency/ConcurrentHashMap.h"
#include "util/absl_util.h"

#include "proto/tracefile.pb.h"
#include "src/perfetto.h"  // IWYU pragma: keep
#include "src/tracefile_reader.h"
#include "src/util.h"

namespace bench {

using proto::Tracefile;
using proto::TraceLine;

template <typename T>
concept IdMapContainer = requires(T t, uint64_t id, void* ptr) {
  { t.SetId(id, ptr) } -> std::convertible_to<bool>;
  { t.GetId(id) } -> std::convertible_to<void*>;
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
      TRACE_EVENT("test_infrastructure", "ConcurrentIdMap::AddAllocation");
      auto [it, inserted] =
          id_map_.insert(id, MapVal{ .allocated_ptr = allocated_ptr });
      if (!inserted) {
        TRACE_EVENT("test_infrastructure", "ConcurrentIdMap::Queue");
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

    // Suspends an allocation that was previously not able to execute. This will
    // atomically check for an allocation made under `id`, and if not found will
    // insert `idx` into the id map as a dependent operation and return true. If
    // an allocation was found to be made, this will return false.
    bool MaybeSuspendAllocation(uint64_t id,
                                std::pair<const TraceLine*, uint64_t> idx) {
      auto [it, inserted] = id_map_.insert(id, MapVal{ .idx = idx });
      return inserted;
    }

    size_t TakeFromQueue(std::pair<const TraceLine*, uint64_t> (&array)[],
                         size_t array_len) {
      TRACE_EVENT("test_infrastructure", "ConcurrentIdMap::TakeFromQueue");
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

  static uint64_t UniqueId(uint64_t id, uint64_t iteration,
                           const Tracefile& tracefile) {
    return id + iteration * tracefile.lines_size();
  }

  uint64_t UniqueId(uint64_t id, uint64_t iteration) {
    return UniqueId(id, iteration, reader_.Tracefile());
  }

  template <IdMapContainer IdMap>
  absl::Status DoMalloc(const TraceLine::Malloc& malloc, uint64_t iteration,
                        IdMap& id_map);

  template <IdMapContainer IdMap>
  absl::Status DoCalloc(const TraceLine::Calloc& calloc, uint64_t iteration,
                        IdMap& id_map);

  template <IdMapContainer IdMap>
  absl::Status DoRealloc(const TraceLine::Realloc& realloc, uint64_t iteration,
                         IdMap& id_map);

  template <IdMapContainer IdMap>
  absl::Status DoFree(const TraceLine::Free& free, uint64_t iteration,
                      IdMap& id_map);

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
absl::Status TracefileExecutor<Allocator>::DoMalloc(
    const TraceLine::Malloc& malloc, uint64_t iteration, IdMap& id_map) {
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
absl::Status TracefileExecutor<Allocator>::DoCalloc(
    const TraceLine::Calloc& calloc, uint64_t iteration, IdMap& id_map) {
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
absl::Status TracefileExecutor<Allocator>::DoRealloc(
    const TraceLine::Realloc& realloc, uint64_t iteration, IdMap& id_map) {
  void* input_ptr;
  if (realloc.has_input_id()) {
    uint64_t unique_id = UniqueId(realloc.input_id(), iteration);
    input_ptr = id_map.GetId(unique_id);
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
absl::Status TracefileExecutor<Allocator>::DoFree(const TraceLine::Free& free,
                                                  uint64_t iteration,
                                                  IdMap& id_map) {
  if (!free.has_input_id()) {
    return allocator_.Free(nullptr, std::nullopt, std::nullopt);
  }

  uint64_t unique_id = UniqueId(free.input_id(), iteration);
  void* ptr = id_map.GetId(unique_id);

  std::optional<size_t> size_hint = free.has_input_size_hint()
                                        ? std::optional(free.input_size_hint())
                                        : std::nullopt;
  std::optional<size_t> alignment_hint =
      free.has_input_alignment_hint()
          ? std::optional(free.input_alignment_hint())
          : std::nullopt;
  RETURN_IF_ERROR(allocator_.Free(ptr, size_hint, alignment_hint));
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
    void* GetId(uint64_t id) const {
      return id_map[id];
    }
  };

  size_t max_simultaneous_allocs =
      reader_.Tracefile().max_simultaneous_allocs();
  VectorIdMap id_map{ .id_map = std::vector<void*>(max_simultaneous_allocs) };

  absl::Time start = absl::Now();
  for (uint64_t t = 0; t < num_repetitions; t++) {
    for (const TraceLine& line : reader_) {
      // Note: iteration should be kept 0 here. It is used to distinguish
      // repeated operations in multithreaded mode when repeating a trace, but
      // this problem does not exist for single-threaded mode. Since indices
      // here are into a vector, and not a hash map, they must be contained in
      // the expected range.
      RETURN_IF_ERROR(ProcessLine(line, /*iteration=*/0, id_map));
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
    LocalIdMap(std::atomic<uint64_t>& idx, const Tracefile& tracefile,
               ConcurrentIdMap& global_id_map, uint64_t num_repetitions)
        : idx_(idx),
          tracefile_(tracefile),
          num_repetitions_(num_repetitions),
          global_id_map_(global_id_map) {
      id_map_.reserve(kBatchSize);
    }

    bool SetId(uint64_t id, void* ptr) {
      auto [it, inserted] = id_map_.insert({ id, ptr });
      return inserted;
    }

    void* GetId(uint64_t id) {
      auto it = id_map_.find(id);
      if (it != id_map_.end()) {
        return it->second;
      }

      auto result = global_id_map_.LookupAllocation(id);
      if (result.has_value()) {
        return result.value();
      }

      // This should not be possible
      std::abort();
      return nullptr;
    }

    size_t PrepareOpsFromTrace(size_t num_trace_ops_to_take,
                               std::pair<const TraceLine*, uint64_t>* ops) {
      TRACE_EVENT("test_infrastructure", "TracefileExecutor::TakingFromTrace");
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

    absl::Status FlushOps(size_t num_ops,
                          std::pair<const TraceLine*, uint64_t>* ops) {
      TRACE_EVENT("test_infrastructure", "LocalIdMap::FlushOps");
      {
        TRACE_EVENT("test_infrastructure", "AddAllocations");
        for (const auto [id, ptr] : id_map_) {
          RETURN_IF_ERROR(global_id_map_.AddAllocation(id, ptr));
        }
      }
      {
        TRACE_EVENT("test_infrastructure", "AddFrees");
        for (size_t i = 0; i < num_ops; i++) {
          auto [line, iteration] = ops[i];
          RETURN_IF_ERROR(EraseFreedAlloc(*line, iteration));
        }
      }
      id_map_.clear();
      return absl::OkStatus();
    }

   private:
    // Checks if an operation will be possible, given the set of local
    // allocations (i.e. allocations made by this thread so far since the last
    // sync) and already-committed global allocations. If this returns false,
    // then `line` is placed in the global queue and can be skipped for now.
    bool CanDoOpOrQueue(absl::flat_hash_set<uint64_t>& local_allocations,
                        const TraceLine& line, uint64_t iteration) {
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

      if (input_id.has_value()) {
        uint64_t id = UniqueId(input_id.value(), iteration, tracefile_);
        auto local_it = local_allocations.find(id);
        if (local_it != local_allocations.end()) {
          local_allocations.erase(local_it);
        } else if (global_id_map_.MaybeSuspendAllocation(
                       id, { &line, iteration })) {
          // If the allocation was suspended, then its dependent operation has
          // not been completed yet. We shouldn't try to perform this op, so
          // skip it.
          return false;
        }
      }
      if (result_id.has_value()) {
        local_allocations.insert(
            UniqueId(result_id.value(), iteration, tracefile_));
      }
      return true;
    }

    // If this operation freed any memory, this will erase the associated
    // metadata in the global id map.
    absl::Status EraseFreedAlloc(const TraceLine& line, uint64_t iteration) {
      uint64_t input_id;
      switch (line.op_case()) {
        case TraceLine::kMalloc:
        case TraceLine::kCalloc: {
          return absl::OkStatus();
        }
        case TraceLine::kRealloc: {
          const TraceLine::Realloc& realloc = line.realloc();
          if (!realloc.has_input_id()) {
            return absl::OkStatus();
          }
          input_id = realloc.input_id();
          break;
        }
        case TraceLine::kFree: {
          const TraceLine::Free& free = line.free();
          if (!free.has_input_id()) {
            return absl::OkStatus();
          }
          input_id = free.input_id();
          break;
        }
        case TraceLine::OP_NOT_SET: {
          return absl::InternalError(
              "Unexpected OP_NO_SET in EraseFreedAlloc()");
        }
      }

      uint64_t id = UniqueId(input_id, iteration, tracefile_);
      RETURN_IF_ERROR(global_id_map_.AddFree(id));
      return absl::OkStatus();
    }

    std::atomic<uint64_t>& idx_;
    const Tracefile& tracefile_;
    const uint64_t num_repetitions_;
    ConcurrentIdMap& global_id_map_;
    // TODO: have trace line batches preprocessed to mutate a local array (i.e.
    // prefetch dependent ID allocations from the global id map in prep stage,
    // then can use the same IdMap as single-threaded executor).
    absl::flat_hash_map<uint64_t, void*> id_map_;
  };

  absl::Duration time;

  LocalIdMap local_id_map(idx, tracefile, global_id_map, num_repetitions);
  while (!done.load(std::memory_order_relaxed)) {
    std::pair<const TraceLine*, uint64_t> ops[kBatchSize];
    size_t total_ops_taken;
    {
      TRACE_EVENT("test_infrastructure", "TracefileExecutor::PrepareWork");
      const size_t queued_ops_taken =
          global_id_map.TakeFromQueue(ops, kMaxQueuedOpsTaken);
      const size_t trace_ops_taken = local_id_map.PrepareOpsFromTrace(
          kBatchSize - queued_ops_taken, &ops[queued_ops_taken]);

      total_ops_taken = queued_ops_taken + trace_ops_taken;
      if (total_ops_taken == 0) {
        break;
      }
    }

    barrier.arrive_and_wait();

    {
      TRACE_EVENT("test_infrastructure", "TracefileExecutor::MeasureAllocator");
      absl::Time start = absl::Now();
      for (size_t i = 0; i < total_ops_taken; i++) {
        auto [line, iteration] = ops[i];
        RETURN_IF_ERROR(ProcessLine(*line, iteration, local_id_map));
      }
      absl::Time end = absl::Now();
      time += end - start;
    }

    RETURN_IF_ERROR(local_id_map.FlushOps(total_ops_taken, ops));
    barrier.arrive_and_wait();
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
      return DoMalloc(line.malloc(), iteration, id_map);
    }
    case TraceLine::kCalloc: {
      return DoCalloc(line.calloc(), iteration, id_map);
    }
    case TraceLine::kRealloc: {
      return DoRealloc(line.realloc(), iteration, id_map);
    }
    case TraceLine::kFree: {
      return DoFree(line.free(), iteration, id_map);
    }
    case TraceLine::OP_NOT_SET: {
      __builtin_unreachable();
    }
  }
}

}  // namespace bench

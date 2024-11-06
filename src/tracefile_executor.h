#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <thread>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "folly/concurrency/ConcurrentHashMap.h"
#include "util/absl_util.h"

#include "proto/tracefile.pb.h"
#include "src/tracefile_reader.h"
#include "src/util.h"

namespace bench {

using proto::Tracefile;
using proto::TraceLine;

template <typename T>
concept IdMapContainer = requires(T t, uint64_t id, void* ptr) {
  { t.SetId(id, ptr) } -> std::convertible_to<bool>;
  { t.GetId(id) } -> std::convertible_to<std::optional<void*>>;
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
  absl::Status Run(
      const TracefileExecutorOptions& options = TracefileExecutorOptions());

  absl::Status RunRepeated(
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
      auto [it, inserted] = id_map_.insert(id, allocated_ptr);
      if (!inserted) {
        {
          absl::MutexLock lock(&queue_lock_);
          queued_idxs_.push_back(it->second);
        }

        auto result = id_map_.assign(id, allocated_ptr);
        if (!result.has_value()) {
          return absl::InternalError(absl::StrFormat(
              "Failed to insert %v after queuing pending operation.", id));
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

    // Looks up a pointer associated with an allocation with given `id`,
    // returning that pointer if one is found. If no allocation is found, `op`
    // is inserted in the map at this id, and `std::nullopt` is returned.
    std::optional<void*> LookupOrQueueAllocation(
        uint64_t id, std::pair<uint64_t, uint64_t> idx) {
      auto [it, inserted] = id_map_.insert(id, std::move(idx));
      if (inserted) {
        return std::nullopt;
      }
      return (*it).allocated_ptr;
    }

   private:
    union MapVal {
      void* allocated_ptr;
      std::pair<uint64_t, uint64_t> idx;
    };

    folly::ConcurrentHashMap<uint64_t, MapVal> id_map_;
    absl::Mutex queue_lock_;
    std::deque<std::pair<uint64_t, uint64_t>> queued_idxs_
        BENCH_GUARDED_BY(queue_lock_);
  };

  struct HashIdMap2 {
    folly::ConcurrentHashMap<uint64_t, void*> id_map;

    bool SetId(uint64_t id, void* ptr) {
      auto [it, inserted] = id_map.insert({ id, ptr });
      return inserted;
    }
    std::optional<void*> GetId(uint64_t id) const {
      auto it = id_map.find(id);
      return it != id_map.end() ? std::optional(it->second) : std::nullopt;
    }
    size_t ClearId(uint64_t id) {
      return id_map.erase(id);
    }
  };

  uint64_t UniqueId(uint64_t id, uint64_t iteration) {
    return id + iteration * reader_.size();
  }

  template <IdMapContainer IdMap>
  absl::Status DoMalloc(const TraceLine::Malloc& malloc, uint64_t iteration,
                        IdMap& id_map);

  template <IdMapContainer IdMap>
  absl::Status DoCalloc(const TraceLine::Calloc& calloc, uint64_t iteration,
                        IdMap& id_map);

  template <IdMapContainer IdMap>
  absl::StatusOr<bool> DoRealloc(const TraceLine::Realloc& realloc,
                                 uint64_t iteration, IdMap& id_map);

  template <IdMapContainer IdMap>
  absl::StatusOr<bool> DoFree(const TraceLine::Free& free, uint64_t iteration,
                              IdMap& id_map);

  absl::Status ProcessTracefile(uint64_t num_repetitions);

  absl::Status ProcessTracefileMultithreaded(
      uint64_t num_repetitions, const TracefileExecutorOptions& options);

  absl::Status ProcessorWorker(std::atomic<size_t>& idx,
                               std::atomic<bool>& done,
                               const Tracefile& tracefile,
                               ConcurrentIdMap& id_map_container,
                               uint64_t num_repetitions);

  static absl::Status RewriteIdsToUnique(Tracefile& tracefile);

  template <IdMapContainer IdMap>
  absl::StatusOr<bool> ProcessLine(const TraceLine& line, uint64_t iteration,
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
absl::Status TracefileExecutor<Allocator>::Run(
    const TracefileExecutorOptions& options) {
  return RunRepeated(/*num_repetitions=*/1, options);
}

template <TracefileAllocator Allocator>
absl::Status TracefileExecutor<Allocator>::RunRepeated(
    uint64_t num_repetitions, const TracefileExecutorOptions& options) {
  RETURN_IF_ERROR(allocator_.InitializeHeap());

  absl::Status result;
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
absl::StatusOr<bool> TracefileExecutor<Allocator>::DoRealloc(
    const TraceLine::Realloc& realloc, uint64_t iteration, IdMap& id_map) {
  void* input_ptr;
  if (realloc.has_input_id()) {
    uint64_t unique_id = UniqueId(realloc.input_id(), iteration);
    std::optional<void*> result = id_map.GetId(unique_id);
    if (!result.has_value()) {
      return false;
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

  return true;
}

template <TracefileAllocator Allocator>
template <IdMapContainer IdMap>
absl::StatusOr<bool> TracefileExecutor<Allocator>::DoFree(
    const TraceLine::Free& free, uint64_t iteration, IdMap& id_map) {
  if (!free.has_input_id()) {
    RETURN_IF_ERROR(allocator_.Free(nullptr, std::nullopt, std::nullopt));
    return true;
  }

  uint64_t unique_id = UniqueId(free.input_id(), iteration);
  std::optional<void*> result = id_map.GetId(unique_id);
  if (!result.has_value()) {
    return false;
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

  return true;
}

template <TracefileAllocator Allocator>
absl::Status TracefileExecutor<Allocator>::ProcessTracefile(
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
    std::optional<void*> GetId(uint64_t id) const {
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

  for (uint64_t t = 0; t < num_repetitions; t++) {
    for (const TraceLine& line : reader_) {
      switch (line.op_case()) {
          // Note: iteration should be kept 0 here. It is used to distinguish
          // repeated operations in multithreaded mode when repeating a trace,
          // but this problem does not exist for single-threaded mode. Since
          // indices here are into a vector, and not a hash map, they must be
          // contained in the expected range.

        case TraceLine::kMalloc: {
          RETURN_IF_ERROR(DoMalloc(line.malloc(), /*iteration=*/0, id_map));
          break;
        }
        case TraceLine::kCalloc: {
          RETURN_IF_ERROR(DoCalloc(line.calloc(), /*iteration=*/0, id_map));
          break;
        }
        case TraceLine::kRealloc: {
          RETURN_IF_ERROR(
              DoRealloc(line.realloc(), /*iteration=*/0, id_map).status());
          break;
        }
        case TraceLine::kFree: {
          RETURN_IF_ERROR(
              DoFree(line.free(), /*iteration=*/0, id_map).status());
          break;
        }
        case TraceLine::OP_NOT_SET: {
          return absl::FailedPreconditionError("Op not set in tracefile");
        }
      }
    }
  }

  return absl::OkStatus();
}

template <TracefileAllocator Allocator>
absl::Status TracefileExecutor<Allocator>::ProcessTracefileMultithreaded(
    uint64_t num_repetitions, const TracefileExecutorOptions& options) {
  Tracefile tracefile(reader_.Tracefile());
  RETURN_IF_ERROR(RewriteIdsToUnique(tracefile));

  absl::Status status = absl::OkStatus();
  absl::Mutex status_lock;

  {
    std::atomic<bool> done = false;

    std::atomic<size_t> idx = 0;
    ConcurrentIdMap id_map_container;

    std::vector<std::thread> threads;
    threads.reserve(options.n_threads);

    for (uint32_t i = 0; i < options.n_threads; i++) {
      threads.emplace_back([this, &status, &status_lock, &done, &idx,
                            &tracefile, &id_map_container, num_repetitions]() {
        auto result = ProcessorWorker(idx, done, tracefile, id_map_container,
                                      num_repetitions);
        if (!result.ok()) {
          done.store(true, std::memory_order_relaxed);

          absl::MutexLock lock(&status_lock);
          if (status.ok()) {
            status = result;
          }
        }
      });
    }

    for (uint32_t i = 0; i < options.n_threads; i++) {
      threads[i].join();
    }
  }

  return status;
}

template <TracefileAllocator Allocator>
absl::Status TracefileExecutor<Allocator>::ProcessorWorker(
    std::atomic<size_t>& idx, std::atomic<bool>& done,
    const Tracefile& tracefile, ConcurrentIdMap& id_map_container,
    uint64_t num_repetitions) {
  static constexpr size_t kBatchSize = 1024;
  static constexpr size_t kQueueProcessLen = 1024;

  struct LocalIdMap {
    const ConcurrentIdMap& global_id_map;
    absl::flat_hash_map<uint64_t, void*> id_map;
    std::vector<uint64_t> erased_ids;

    explicit LocalIdMap(const ConcurrentIdMap& global_id_map)
        : global_id_map(global_id_map) {
      id_map.reserve(std::max(kBatchSize, kQueueProcessLen));
      erased_ids.reserve(std::max(kBatchSize, kQueueProcessLen));
    }

    bool SetId(uint64_t id, void* ptr) {
      auto [it, inserted] = id_map.insert({ id, ptr });
      return inserted;
    }
    std::optional<void*> GetId(uint64_t id) const {
      auto result = global_id_map.GetId(id);
      if (result.has_value()) {
        return result.value();
      }

      auto it = id_map.find(id);
      return it != id_map.end() ? std::optional(it->second) : std::nullopt;
    }
    size_t ClearId(uint64_t id) {
      if (id_map.erase(id) == 0) {
        erased_ids.push_back(id);
      }
      return 1;
    }

    void FlushOps(ConcurrentIdMap& id_map_container) {
      for (const auto [id, ptr] : id_map) {
        bool inserted = id_map_container.SetId(id, ptr);
        (void) inserted;
        assert(inserted);
      }
      for (uint64_t erased_id : erased_ids) {
        size_t erased_elems = id_map_container.ClearId(erased_id);
        (void) erased_elems;
        assert(erased_elems == 1);
      }
      id_map.clear();
      erased_ids.clear();
    }
  };

  bool queue_empty = false;
  bool tracefile_complete = false;

  while (!done.load(std::memory_order_relaxed) &&
         (!queue_empty || !tracefile_complete)) {
    std::pair<uint64_t, uint64_t> idxs[kQueueProcessLen];
    uint32_t iters;
    {
      absl::MutexLock lock(&queue_mutex);
      iters =
          static_cast<uint32_t>(std::min(queued_idxs.size(), kQueueProcessLen));
      for (uint32_t i = 0; i < iters; i++) {
        idxs[i] = queued_idxs.front();
        queued_idxs.pop_front();
      }
    }
    queue_empty = iters == 0;

    LocalIdMap local_id_map(id_map_container);
    size_t first_idx = idx.fetch_add(kBatchSize, std::memory_order_relaxed);
    if (first_idx >= num_repetitions * tracefile.lines_size()) {
      idx.store(num_repetitions * tracefile.lines_size(),
                std::memory_order_relaxed);
      tracefile_complete = true;
    } else {
      for (size_t i = first_idx;
           i < std::min<size_t>(first_idx + kBatchSize,
                                num_repetitions * tracefile.lines_size());
           i++) {
        size_t line_idx = i % tracefile.lines_size();
        uint64_t iteration = i / tracefile.lines_size();
        DEFINE_OR_RETURN(
            bool, succeeded,
            ProcessLine(tracefile.lines(line_idx), iteration, local_id_map));

        if (!succeeded) {
          absl::MutexLock lock(&queue_mutex);
          queued_idxs.emplace_back(line_idx, iteration);
        }
      }
    }

    local_id_map.FlushOps(id_map_container);

    for (uint64_t i = 0; i < iters; i++) {
      auto [line_idx, iteration] = idxs[i];
      DEFINE_OR_RETURN(
          bool, succeeded,
          ProcessLine(tracefile.lines(line_idx), iteration, local_id_map));

      if (!succeeded) {
        absl::MutexLock lock(&queue_mutex);
        queued_idxs.push_back(idxs[i]);
      }
    }

    local_id_map.FlushOps(id_map_container);
  }

  return absl::OkStatus();
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
absl::StatusOr<bool> TracefileExecutor<Allocator>::ProcessLine(
    const TraceLine& line, uint64_t iteration, IdMap& id_map) {
  switch (line.op_case()) {
    case TraceLine::kMalloc: {
      RETURN_IF_ERROR(DoMalloc(line.malloc(), iteration, id_map));
      return true;
    }
    case TraceLine::kCalloc: {
      RETURN_IF_ERROR(DoCalloc(line.calloc(), iteration, id_map));
      return true;
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

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

  Allocator& Inner() {
    return allocator_;
  }
  const Allocator& Inner() const {
    return allocator_;
  }

 private:
  struct HashIdMap {
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

  template <IdMapContainer IdMap>
  absl::Status DoMalloc(const TraceLine::Malloc& malloc, IdMap& id_map);

  template <IdMapContainer IdMap>
  absl::Status DoCalloc(const TraceLine::Calloc& calloc, IdMap& id_map);

  template <IdMapContainer IdMap>
  absl::StatusOr<bool> DoRealloc(const TraceLine::Realloc& realloc,
                                 IdMap& id_map);

  template <IdMapContainer IdMap>
  absl::StatusOr<bool> DoFree(const TraceLine::Free& free, IdMap& id_map);

  absl::Status ProcessTracefile();

  absl::Status ProcessTracefileMultithreaded(
      const TracefileExecutorOptions& options);

  absl::Status ProcessorWorker(std::atomic<size_t>& idx,
                               std::atomic<bool>& done,
                               const Tracefile& tracefile,
                               HashIdMap& id_map_container,
                               absl::Mutex& queue_mutex,
                               std::deque<uint64_t>& queued_idxs);

  static absl::Status RewriteIdsToUnique(Tracefile& tracefile);

  template <IdMapContainer IdMap>
  absl::StatusOr<bool> ProcessLine(const TraceLine& line, IdMap& id_map);

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
  RETURN_IF_ERROR(allocator_.InitializeHeap());

  absl::Status result;
  if (options.n_threads == 1) {
    result = ProcessTracefile();
  } else {
    result = ProcessTracefileMultithreaded(options);
  }

  RETURN_IF_ERROR(allocator_.CleanupHeap());
  return result;
}

template <TracefileAllocator Allocator>
template <IdMapContainer IdMap>
absl::Status TracefileExecutor<Allocator>::DoMalloc(
    const TraceLine::Malloc& malloc, IdMap& id_map) {
  std::optional<size_t> alignment =
      malloc.has_input_alignment() ? std::optional(malloc.input_alignment())
                                   : std::nullopt;
  DEFINE_OR_RETURN(void*, ptr,
                   allocator_.Malloc(malloc.input_size(), alignment));

  if (malloc.input_size() != 0 && malloc.has_result_id()) {
    if (!id_map.SetId(malloc.result_id(), ptr)) {
      return absl::InternalError(absl::StrFormat(
          "Failed to set ID %v for malloc in id map, already exists",
          malloc.result_id()));
    }
  }

  return absl::OkStatus();
}

template <TracefileAllocator Allocator>
template <IdMapContainer IdMap>
absl::Status TracefileExecutor<Allocator>::DoCalloc(
    const TraceLine::Calloc& calloc, IdMap& id_map) {
  DEFINE_OR_RETURN(
      void*, ptr, allocator_.Calloc(calloc.input_nmemb(), calloc.input_size()));

  if (calloc.input_nmemb() != 0 && calloc.input_size() != 0 &&
      calloc.has_result_id()) {
    if (!id_map.SetId(calloc.result_id(), ptr)) {
      return absl::InternalError(absl::StrFormat(
          "Failed to set ID %v for calloc in id map, already exists",
          calloc.result_id()));
    }
  }

  return absl::OkStatus();
}

template <TracefileAllocator Allocator>
template <IdMapContainer IdMap>
absl::StatusOr<bool> TracefileExecutor<Allocator>::DoRealloc(
    const TraceLine::Realloc& realloc, IdMap& id_map) {
  void* input_ptr;
  if (realloc.has_input_id()) {
    std::optional<void*> result = id_map.GetId(realloc.input_id());
    if (!result.has_value()) {
      return false;
    }
    input_ptr = result.value();

    if (id_map.ClearId(realloc.input_id()) != 1) {
      return absl::InternalError(
          absl::StrFormat("ID %v for realloc erased by other concurrent op",
                          realloc.input_id()));
    }
  } else {
    input_ptr = nullptr;
  }
  DEFINE_OR_RETURN(void*, result_ptr,
                   allocator_.Realloc(input_ptr, realloc.input_size()));
  if (!id_map.SetId(realloc.result_id(), result_ptr)) {
    return absl::InternalError(absl::StrFormat(
        "Failed to set ID %v for realloc in id map, already exists",
        realloc.result_id()));
  }

  return true;
}

template <TracefileAllocator Allocator>
template <IdMapContainer IdMap>
absl::StatusOr<bool> TracefileExecutor<Allocator>::DoFree(
    const TraceLine::Free& free, IdMap& id_map) {
  if (!free.has_input_id()) {
    RETURN_IF_ERROR(allocator_.Free(nullptr, std::nullopt, std::nullopt));
    return true;
  }

  std::optional<void*> result = id_map.GetId(free.input_id());
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

  if (id_map.ClearId(free.input_id()) != 1) {
    return absl::InternalError(absl::StrFormat(
        "ID %v for free erased by other concurrent op", free.input_id()));
  }

  return true;
}

template <TracefileAllocator Allocator>
absl::Status TracefileExecutor<Allocator>::ProcessTracefile() {
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

  for (const TraceLine& line : reader_) {
    switch (line.op_case()) {
      case TraceLine::kMalloc: {
        RETURN_IF_ERROR(DoMalloc(line.malloc(), id_map));
        break;
      }
      case TraceLine::kCalloc: {
        RETURN_IF_ERROR(DoCalloc(line.calloc(), id_map));
        break;
      }
      case TraceLine::kRealloc: {
        RETURN_IF_ERROR(DoRealloc(line.realloc(), id_map).status());
        break;
      }
      case TraceLine::kFree: {
        RETURN_IF_ERROR(DoFree(line.free(), id_map).status());
        break;
      }
      case TraceLine::OP_NOT_SET: {
        return absl::FailedPreconditionError("Op not set in tracefile");
      }
    }
  }

  return absl::OkStatus();
}

template <TracefileAllocator Allocator>
absl::Status TracefileExecutor<Allocator>::ProcessTracefileMultithreaded(
    const TracefileExecutorOptions& options) {
  Tracefile tracefile(reader_.Tracefile());
  RETURN_IF_ERROR(RewriteIdsToUnique(tracefile));

  absl::Status status = absl::OkStatus();

  {
    std::atomic<bool> done = false;

    std::atomic<size_t> idx = 0;
    HashIdMap id_map_container;

    absl::Mutex queue_mutex;
    std::deque<uint64_t> queued_idxs;

    std::vector<std::thread> threads;
    threads.reserve(options.n_threads);

    for (uint32_t i = 0; i < options.n_threads; i++) {
      threads.emplace_back([this, &status, &done, &idx, &tracefile,
                            &id_map_container, &queue_mutex, &queued_idxs]() {
        auto result = ProcessorWorker(idx, done, tracefile, id_map_container,
                                      queue_mutex, queued_idxs);
        if (!result.ok()) {
          done.store(true, std::memory_order_relaxed);

          // Use the queue lock, no need to make another lock.
          absl::MutexLock lock(&queue_mutex);
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
    const Tracefile& tracefile, HashIdMap& id_map_container,
    absl::Mutex& queue_mutex, std::deque<uint64_t>& queued_idxs) {
  static constexpr size_t kBatchSize = 32;
  bool queue_empty = false;
  bool tracefile_complete = false;

  while (!done.load(std::memory_order_relaxed) &&
         (!queue_empty || !tracefile_complete)) {
    uint64_t idxs[4];
    uint32_t iters;
    {
      absl::MutexLock lock(&queue_mutex);
      iters = static_cast<uint32_t>(std::min<size_t>(queued_idxs.size(), 4));
      for (uint32_t i = 0; i < iters; i++) {
        idxs[i] = queued_idxs.front();
        queued_idxs.pop_front();
      }
    }
    queue_empty = iters == 0;

    size_t first_idx = idx.fetch_add(kBatchSize, std::memory_order_relaxed);
    if (first_idx >= tracefile.lines_size()) {
      idx.store(tracefile.lines_size(), std::memory_order_relaxed);
      tracefile_complete = true;
    } else {
      for (size_t i = first_idx;
           i < std::min<size_t>(first_idx + kBatchSize, tracefile.lines_size());
           i++) {
        DEFINE_OR_RETURN(bool, succeeded,
                         ProcessLine(tracefile.lines(i), id_map_container));

        if (!succeeded) {
          absl::MutexLock lock(&queue_mutex);
          queued_idxs.push_back(i);
        }
      }
    }

    for (uint64_t i = 0; i < iters; i++) {
      DEFINE_OR_RETURN(bool, succeeded,
                       ProcessLine(tracefile.lines(idxs[i]), id_map_container));

      if (!succeeded) {
        absl::MutexLock lock(&queue_mutex);
        queued_idxs.push_back(idxs[i]);
      }
    }
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
          if (release_it->second.second != 0) {
            realloc.set_input_id(release_it->second.first);
          } else {
            realloc.clear_input_id();
          }
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
        if (release_it->second.second != 0) {
          free.set_input_id(release_it->second.first);
        } else {
          free.clear_input_id();
        }
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
    const TraceLine& line, IdMap& id_map) {
  switch (line.op_case()) {
    case TraceLine::kMalloc: {
      RETURN_IF_ERROR(DoMalloc(line.malloc(), id_map));
      return true;
    }
    case TraceLine::kCalloc: {
      RETURN_IF_ERROR(DoCalloc(line.calloc(), id_map));
      return true;
    }
    case TraceLine::kRealloc: {
      return DoRealloc(line.realloc(), id_map);
    }
    case TraceLine::kFree: {
      return DoFree(line.free(), id_map);
    }
    case TraceLine::OP_NOT_SET: {
      __builtin_unreachable();
    }
  }
}

}  // namespace bench

#include "src/tracefile_executor.h"

#include <atomic>
#include <deque>
#include <thread>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "util/absl_util.h"

#include "proto/tracefile.pb.h"
#include "src/heap_factory.h"
#include "src/tracefile_reader.h"

namespace bench {

using proto::TraceLine;

TracefileExecutor::TracefileExecutor(TracefileReader& reader,
                                     HeapFactory& heap_factory)
    : reader_(reader), heap_factory_(&heap_factory) {}

absl::Status TracefileExecutor::Run(const TracefileExecutorOptions& options) {
  InitializeHeap(*heap_factory_);

  if (options.n_threads == 1) {
    return ProcessTracefile();
  }
  return ProcessTracefileMultithreaded(options);
}

template <IdMapContainer IdMap>
absl::Status TracefileExecutor::DoMalloc(const proto::TraceLine::Malloc& malloc,
                                         IdMap& id_map) {
  std::optional<size_t> alignment =
      malloc.has_input_alignment() ? std::optional(malloc.input_alignment())
                                   : std::nullopt;
  DEFINE_OR_RETURN(void*, ptr, Malloc(malloc.input_size(), alignment));

  if (malloc.input_size() != 0 && malloc.has_result_id()) {
    id_map.SetId(malloc.result_id(), ptr);
  }

  return absl::OkStatus();
}

template <IdMapContainer IdMap>
absl::Status TracefileExecutor::DoCalloc(const proto::TraceLine::Calloc& calloc,
                                         IdMap& id_map) {
  DEFINE_OR_RETURN(void*, ptr,
                   Calloc(calloc.input_nmemb(), calloc.input_size()));

  if (calloc.input_nmemb() != 0 && calloc.input_size() != 0 &&
      calloc.has_result_id()) {
    id_map.SetId(calloc.result_id(), ptr);
  }

  return absl::OkStatus();
}

template <IdMapContainer IdMap>
absl::StatusOr<bool> TracefileExecutor::DoRealloc(
    const proto::TraceLine::Realloc& realloc, IdMap& id_map) {
  void* input_ptr;
  if (realloc.has_input_id()) {
    std::optional<void*> result = id_map.GetId(realloc.input_id());
    if (!result.has_value()) {
      return false;
    }
    input_ptr = result.value();
  } else {
    input_ptr = nullptr;
  }
  DEFINE_OR_RETURN(void*, result_ptr, Realloc(input_ptr, realloc.input_size()));
  id_map.SetId(realloc.result_id(), result_ptr);

  return true;
}

template <IdMapContainer IdMap>
absl::StatusOr<bool> TracefileExecutor::DoFree(
    const proto::TraceLine::Free& free, IdMap& id_map) {
  if (!free.has_input_id()) {
    RETURN_IF_ERROR(Free(nullptr, std::nullopt, std::nullopt));
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
  RETURN_IF_ERROR(Free(ptr, size_hint, alignment_hint));
  return true;
}

absl::Status TracefileExecutor::ProcessTracefile() {
  // A map from allocation id's to pointers returned from the allocator. Since
  // id's are assigned contiguously from lowest to highest ID, they can be
  // stored in a vector.
  struct VectorIdMap {
    std::vector<void*> id_map;

    void SetId(uint64_t id, void* ptr) {
      id_map[id] = ptr;
    }
    std::optional<void*> GetId(uint64_t id) const {
      return id_map[id];
    }
  };
  VectorIdMap id_map{ std::vector<void*>(
      reader_.Tracefile().max_simultaneous_allocs()) };

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

absl::Status TracefileExecutor::ProcessTracefileMultithreaded(
    const TracefileExecutorOptions& options) {
  struct HashIdMap {
    absl::flat_hash_map<uint64_t, void*>& id_map;
    absl::Mutex& mutex;

    void SetId(uint64_t id, void* ptr) {
      absl::MutexLock guard(&mutex);
      id_map[id] = ptr;
    }
    std::optional<void*> GetId(uint64_t id) const {
      absl::MutexLock guard(&mutex);
      auto it = id_map.find(id);
      return it != id_map.end() ? std::optional(it->second) : std::nullopt;
    }
  };

  Tracefile tracefile(reader_.Tracefile());
  RETURN_IF_ERROR(RewriteIdsToUnique(tracefile));

  {
    std::vector<std::jthread> threads;
    threads.reserve(options.n_threads);

    std::atomic<uint64_t> idx;
    absl::Mutex id_map_mutex;
    absl::flat_hash_map<uint64_t, void*> id_map;
    HashIdMap id_map_container{ id_map, id_map_mutex };

    absl::Mutex queue_mutex;
    std::deque<uint64_t> queued_idxs;

    for (uint32_t i = 0; i < options.n_threads; i++) {
      threads.emplace_back([this, &idx, &tracefile, &id_map_container,
                            &queue_mutex, &queued_idxs]() {
        static constexpr uint64_t kBatchSize = 32;
        HashIdMap id_map = id_map_container;
        bool queue_empty = false;
        bool tracefile_complete = false;

        while (!queue_empty || !tracefile_complete) {
          uint64_t idxs[4];
          uint32_t iters = 0;
          {
            absl::MutexLock lock(&queue_mutex);
            if (!queued_idxs.empty()) {
              iters = std::min<uint32_t>(queued_idxs.size(), 4);
              for (uint32_t i = 0; i < iters; i++) {
                idxs[i] = queued_idxs.front();
                queued_idxs.pop_front();
              }
            }
          }
          queue_empty = iters == 0;

          uint64_t first_idx =
              idx.fetch_add(kBatchSize, std::memory_order_relaxed);
          if (first_idx >= tracefile.lines_size()) {
            idx.store(tracefile.lines_size(), std::memory_order_relaxed);
            tracefile_complete = true;
          } else {
            for (uint64_t i = first_idx;
                 i < std::min<uint64_t>(first_idx + kBatchSize,
                                        tracefile.lines_size());
                 i++) {
              auto result = ProcessLine(tracefile.lines(i), id_map);
              if (!result.ok()) {
                std::cerr << result << std::endl;
                std::abort();
              }

              if (!result.value()) {
                absl::MutexLock lock(&queue_mutex);
                queued_idxs.push_back(i);
              }
            }
          }

          for (uint64_t i = 0; i < iters; i++) {
            auto result = ProcessLine(tracefile.lines(idxs[i]), id_map);
            if (!result.ok()) {
              std::cerr << result << std::endl;
              return;
            }

            if (!result.value()) {
              absl::MutexLock lock(&queue_mutex);
              queued_idxs.push_back(i);
            }
          }
        }
      });
    }
  }

  return absl::OkStatus();
}

/* static */
absl::Status TracefileExecutor::RewriteIdsToUnique(
    proto::Tracefile& tracefile) {
  uint64_t next_id = 0;
  absl::flat_hash_map<uint64_t, uint64_t> new_id_map;

  for (TraceLine& line : *tracefile.mutable_lines()) {
    switch (line.op_case()) {
      case TraceLine::kMalloc: {
        TraceLine::Malloc& malloc = *line.mutable_malloc();
        auto [it, inserted] =
            new_id_map.insert({ malloc.result_id(), next_id });
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
        auto [it, inserted] =
            new_id_map.insert({ calloc.result_id(), next_id });
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
        auto release_it = new_id_map.find(realloc.input_id());
        if (release_it == new_id_map.end()) {
          return absl::FailedPreconditionError(absl::StrFormat(
              "Unknown ID being realloc-ed: %v", realloc.input_id()));
        }
        realloc.set_input_id(release_it->second);

        auto [it, inserted] =
            new_id_map.insert({ realloc.result_id(), next_id });
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
        auto release_it = new_id_map.find(free.input_id());
        if (release_it == new_id_map.end()) {
          return absl::FailedPreconditionError(
              absl::StrFormat("Unknown ID being freed: %v", free.input_id()));
        }
        free.set_input_id(release_it->second);
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

template <IdMapContainer IdMap>
absl::StatusOr<bool> TracefileExecutor::ProcessLine(
    const proto::TraceLine& line, IdMap& id_map) {
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

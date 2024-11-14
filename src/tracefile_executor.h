#pragma once

#include <atomic>
#include <barrier>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <thread>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "util/absl_util.h"

#include "proto/tracefile.pb.h"
#include "src/concurrent_id_map.h"
#include "src/local_id_map.h"
#include "src/perfetto.h"  // IWYU pragma: keep
#include "src/tracefile_reader.h"

namespace bench {

using proto::Tracefile;
using proto::TraceLine;

// A map from allocation id's to pointers returned from the allocator. Since
// id's are assigned contiguously from lowest to highest ID, they can be
// stored in an array.
struct IdMap {
  void** const id_map;

  void SetId(uint64_t id, void* ptr) const {
    id_map[id] = ptr;
  }
  void* GetId(uint64_t id) const {
    return id_map[id];
  }
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
  uint64_t UniqueId(uint64_t id, uint64_t iteration) {
    return ConcurrentIdMap::UniqueId(id, iteration, reader_.Tracefile());
  }

  absl::Status DoMalloc(const TraceLine::Malloc& malloc, IdMap& id_map);

  absl::Status DoCalloc(const TraceLine::Calloc& calloc, IdMap& id_map);

  absl::Status DoRealloc(const TraceLine::Realloc& realloc, IdMap& id_map);

  absl::Status DoFree(const TraceLine::Free& free, IdMap& id_map);

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

  absl::Status ProcessLine(const TraceLine& line, IdMap& id_map);

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
absl::Status TracefileExecutor<Allocator>::DoMalloc(
    const TraceLine::Malloc& malloc, IdMap& id_map) {
  std::optional<size_t> alignment =
      malloc.has_input_alignment() ? std::optional(malloc.input_alignment())
                                   : std::nullopt;
  DEFINE_OR_RETURN(void*, ptr,
                   allocator_.Malloc(malloc.input_size(), alignment));

  if (malloc.input_size() != 0 && malloc.has_result_id()) {
    id_map.SetId(malloc.result_id(), ptr);
  }

  return absl::OkStatus();
}

template <TracefileAllocator Allocator>
absl::Status TracefileExecutor<Allocator>::DoCalloc(
    const TraceLine::Calloc& calloc, IdMap& id_map) {
  DEFINE_OR_RETURN(
      void*, ptr, allocator_.Calloc(calloc.input_nmemb(), calloc.input_size()));

  if (calloc.input_nmemb() != 0 && calloc.input_size() != 0 &&
      calloc.has_result_id()) {
    id_map.SetId(calloc.result_id(), ptr);
  }

  return absl::OkStatus();
}

template <TracefileAllocator Allocator>
absl::Status TracefileExecutor<Allocator>::DoRealloc(
    const TraceLine::Realloc& realloc, IdMap& id_map) {
  void* input_ptr;
  if (realloc.has_input_id()) {
    input_ptr = id_map.GetId(realloc.input_id());
  } else {
    input_ptr = nullptr;
  }
  DEFINE_OR_RETURN(void*, result_ptr,
                   allocator_.Realloc(input_ptr, realloc.input_size()));
  id_map.SetId(realloc.result_id(), result_ptr);

  return absl::OkStatus();
}

template <TracefileAllocator Allocator>
absl::Status TracefileExecutor<Allocator>::DoFree(const TraceLine::Free& free,
                                                  IdMap& id_map) {
  if (!free.has_input_id()) {
    return allocator_.Free(nullptr, std::nullopt, std::nullopt);
  }

  void* ptr = id_map.GetId(free.input_id());

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
  size_t max_simultaneous_allocs =
      reader_.Tracefile().max_simultaneous_allocs();
  std::vector<void*> id_map_vec(max_simultaneous_allocs);
  IdMap id_map{ .id_map = id_map_vec.data() };

  absl::Time start = absl::Now();
  for (uint64_t t = 0; t < num_repetitions; t++) {
    for (const TraceLine& line : reader_) {
      RETURN_IF_ERROR(ProcessLine(line, id_map));
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
  absl::Duration time;

  LocalIdMap local_id_map(idx, tracefile, global_id_map, num_repetitions);
  while (!done.load(std::memory_order_relaxed)) {
    DEFINE_OR_RETURN(LocalIdMap::BatchContext, context,
                     local_id_map.PrepareBatch());
    if (context.NumOps() == 0) {
      break;
    }

    // Wait at the barrier twice to maximize chance of allocator measuring
    // routines to start simultaneously. Threads waiting at a barrier for a long
    // time will suspend and may take a long time to reschedule, so waiting at
    // the barrier a second time makes it more likely all threads are ready to
    // schedule soon.
    barrier.arrive_and_wait();
    barrier.arrive_and_wait();

    {
      TRACE_EVENT("test_infrastructure", "TracefileExecutor::MeasureAllocator");

      IdMap id_map{ .id_map = context.IdMap().data() };
      absl::Time start = absl::Now();
      for (const auto& line : context.Ops()) {
        RETURN_IF_ERROR(ProcessLine(line, id_map));
      }
      absl::Time end = absl::Now();
      time += end - start;
    }

    RETURN_IF_ERROR(local_id_map.FlushOps(context));
    barrier.arrive_and_wait();
  }

  barrier.arrive_and_drop();
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
absl::Status TracefileExecutor<Allocator>::ProcessLine(const TraceLine& line,
                                                       IdMap& id_map) {
  switch (line.op_case()) {
    case TraceLine::kMalloc: {
      return DoMalloc(line.malloc(), id_map);
    }
    case TraceLine::kCalloc: {
      return DoCalloc(line.calloc(), id_map);
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

#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "folly/concurrency/ConcurrentHashMap.h"

#include "proto/tracefile.pb.h"
#include "src/util.h"

namespace bench {

using proto::Tracefile;
using proto::TraceLine;

class ConcurrentIdMap {
 public:
  // Given an id from a tracefile (which must be unique within the trace),
  // generates a globally unique ID across multiple repetitions of the trace
  // (where `iteration` is the current iteration over the trace).
  static uint64_t UniqueId(uint64_t id, uint64_t iteration,
                           const Tracefile& tracefile);

  // Adds an allocation to the map. Returns a failure status if it failed
  // because the key `id` was already in use.
  absl::Status AddAllocation(uint64_t id, void* allocated_ptr);

  // Removes a tracked allocation from the map (because it was freed). Returns
  // an error status if the removal failed because the key `id` was not found.
  absl::Status AddFree(uint64_t id);

  // Looks up an allocation by ID, returning the pointer allocated with this
  // ID if it exists, otherwise `std::nullopt`.
  std::optional<void*> LookupAllocation(uint64_t id);

  // Suspends an allocation that was previously not able to execute. This will
  // atomically check for an allocation made under `id`, and if not found will
  // insert `idx` into the id map as a dependent operation and return true. If
  // an allocation was found to be made, this will return false.
  bool MaybeSuspendAllocation(uint64_t id,
                              std::pair<const TraceLine*, uint64_t> idx);

  size_t TakeFromQueue(std::pair<const TraceLine*, uint64_t> (&array)[],
                       size_t array_len);

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

}  // namespace bench

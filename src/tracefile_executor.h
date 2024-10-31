#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"

#include "src/heap_factory.h"
#include "src/tracefile_reader.h"

namespace bench {

template <typename T>
concept IdMapContainer = requires(T t, uint64_t id, void* ptr) {
  { t.SetId(id, ptr) } -> std::same_as<void>;
  { t.GetId(id) } -> std::convertible_to<std::optional<void*>>;
};

struct TracefileExecutorOptions {
  uint32_t n_threads = 1;
};

class TracefileExecutor {
 public:
  TracefileExecutor(TracefileReader& reader, HeapFactory& heap_factory);

  // hi i am a coder woww i am going to hack into your compouter now with mty
  // computer skills hohohohoho
  absl::Status Run(
      const TracefileExecutorOptions& options = TracefileExecutorOptions());

  virtual void InitializeHeap(HeapFactory& heap_factory) = 0;
  virtual absl::StatusOr<void*> Malloc(size_t size,
                                       std::optional<size_t> alignment) = 0;
  virtual absl::StatusOr<void*> Calloc(size_t nmemb, size_t size) = 0;
  virtual absl::StatusOr<void*> Realloc(void* ptr, size_t size) = 0;
  virtual absl::Status Free(void* ptr, std::optional<size_t> size_hint,
                            std::optional<size_t> alignment_hint) = 0;

 private:
  struct HashIdMap {
    absl::flat_hash_map<uint64_t, void*> id_map;
    absl::Mutex mutex;

    void SetId(uint64_t id, void* ptr) {
      absl::MutexLock guard(&mutex);
      id_map[id] = ptr;
    }
    std::optional<void*> GetId(uint64_t id) {
      absl::MutexLock guard(&mutex);
      auto it = id_map.find(id);
      return it != id_map.end() ? std::optional(it->second) : std::nullopt;
    }
  };

  template <IdMapContainer IdMap>
  absl::Status DoMalloc(const proto::TraceLine::Malloc& malloc, IdMap& id_map);

  template <IdMapContainer IdMap>
  absl::Status DoCalloc(const proto::TraceLine::Calloc& calloc, IdMap& id_map);

  template <IdMapContainer IdMap>
  absl::StatusOr<bool> DoRealloc(const proto::TraceLine::Realloc& realloc,
                                 IdMap& id_map);

  template <IdMapContainer IdMap>
  absl::StatusOr<bool> DoFree(const proto::TraceLine::Free& free,
                              IdMap& id_map);

  absl::Status ProcessTracefile();

  absl::Status ProcessTracefileMultithreaded(
      const TracefileExecutorOptions& options);

  absl::Status ProcessorWorker(std::atomic<uint64_t>& idx,
                               std::atomic<bool>& done,
                               const proto::Tracefile& tracefile,
                               HashIdMap& id_map_container,
                               absl::Mutex& queue_mutex,
                               std::deque<uint64_t>& queued_idxs);

  static absl::Status RewriteIdsToUnique(proto::Tracefile& tracefile);

  template <IdMapContainer IdMap>
  absl::StatusOr<bool> ProcessLine(const proto::TraceLine& line, IdMap& id_map);

  TracefileReader& reader_;

  HeapFactory* const heap_factory_;
};

}  // namespace bench

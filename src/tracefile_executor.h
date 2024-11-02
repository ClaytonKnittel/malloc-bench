#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "folly/concurrency/ConcurrentHashMap.h"

#include "src/heap_factory.h"
#include "src/tracefile_reader.h"

namespace bench {

template <typename T>
concept IdMapContainer = requires(T t, uint64_t id, void* ptr) {
  { t.SetId(id, ptr) } -> std::convertible_to<bool>;
  { t.GetId(id) } -> std::convertible_to<std::optional<void*>>;
  { t.ClearId(id) } -> std::convertible_to<size_t>;
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
  virtual absl::Status CleanupHeap() = 0;
  virtual absl::StatusOr<void*> Malloc(size_t size,
                                       std::optional<size_t> alignment) = 0;
  virtual absl::StatusOr<void*> Calloc(size_t nmemb, size_t size) = 0;
  virtual absl::StatusOr<void*> Realloc(void* ptr, size_t size) = 0;
  virtual absl::Status Free(void* ptr, std::optional<size_t> size_hint,
                            std::optional<size_t> alignment_hint) = 0;

  HeapFactory& HeapFactoryRef() {
    return *heap_factory_;
  }
  const HeapFactory& HeapFactoryRef() const {
    return *heap_factory_;
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

  absl::Status ProcessorWorker(std::atomic<size_t>& idx,
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

#pragma once

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "folly/concurrency/ConcurrentHashMap.h"

#include "src/heap_factory.h"
#include "src/malloc_runner.h"
#include "src/tracefile_executor.h"
#include "src/tracefile_reader.h"

namespace bench {

class Utiltest : public MallocRunner<size_t> {
 public:
  explicit Utiltest(HeapFactory& heap_factory);

  static absl::StatusOr<double> MeasureUtilization(
      TracefileReader& reader, HeapFactory& heap_factory,
      const TracefileExecutorOptions& options = TracefileExecutorOptions());

  absl::Status PostAlloc(void* ptr, size_t size,
                         std::optional<size_t> alignment,
                         bool is_calloc) override;

  absl::StatusOr<size_t> PreRealloc(void* ptr, size_t size) override;

  absl::Status PostRealloc(void* new_ptr, void* old_ptr, size_t size,
                           size_t prev_size) override;

  absl::Status PreRelease(void* ptr) override;

 private:
  void RecomputeMax(size_t total_allocated_bytes);

  absl::StatusOr<double> ComputeUtilization() const;

  folly::ConcurrentHashMap<void*, size_t> size_map_;

  std::atomic<size_t> total_allocated_bytes_ = 0;
  std::atomic<size_t> max_allocated_bytes_ = 0;
  std::atomic<size_t> max_heap_size_ = 0;
};

}  // namespace bench

#pragma once

#include "absl/status/statusor.h"
#include "folly/AtomicHashMap.h"

#include "src/heap_factory.h"
#include "src/malloc_runner.h"
#include "src/tracefile_executor.h"
#include "src/tracefile_reader.h"

namespace bench {

class Utiltest : private MallocRunner {
 public:
  static absl::StatusOr<double> MeasureUtilization(
      TracefileReader& reader, HeapFactory& heap_factory,
      const TracefileExecutorOptions& options = TracefileExecutorOptions());

 private:
  Utiltest(TracefileReader& reader, HeapFactory& heap_factory);

  absl::Status PostAlloc(void* ptr, size_t size,
                         std::optional<size_t> alignment,
                         bool is_calloc) override;

  absl::Status PreRealloc(void* ptr, size_t size) override;

  absl::Status PostRealloc(void* new_ptr, void* old_ptr, size_t size) override;

  absl::Status PreRelease(void* ptr) override;

  void RecomputeMax(size_t total_allocated_bytes);

  absl::StatusOr<double> ComputeUtilization() const;

  folly::AtomicHashMap<void*, size_t> size_map_;

  std::atomic<size_t> total_allocated_bytes_ = 0;
  std::atomic<size_t> max_allocated_bytes_ = 0;
  std::atomic<size_t> max_heap_size_ = 0;
};

}  // namespace bench

#pragma once

#include <cstddef>
#include <optional>

#include "src/heap_factory.h"
#include "src/tracefile_executor.h"  // IWYU pragma: keep

namespace bench {

class MallocRunner {
 public:
  static constexpr char kFailedTestPrefix[] = "[Failed]";

  explicit MallocRunner(HeapFactory& heap_factory, bool verbose = false);

  virtual absl::Status PostAlloc(void* ptr, size_t size,
                                 std::optional<size_t> alignment,
                                 bool is_calloc) = 0;

  virtual absl::Status PreRealloc(void* ptr, size_t size) = 0;

  virtual absl::Status PostRealloc(void* new_ptr, void* old_ptr,
                                   size_t size) = 0;

  virtual absl::Status PreRelease(void* ptr) = 0;

  absl::Status InitializeHeap();
  absl::Status CleanupHeap();
  absl::StatusOr<void*> Malloc(size_t size, std::optional<size_t> alignment);
  absl::StatusOr<void*> Calloc(size_t nmemb, size_t size);
  absl::StatusOr<void*> Realloc(void* ptr, size_t size);
  absl::Status Free(void* ptr, std::optional<size_t> size_hint,
                    std::optional<size_t> alignment_hint);

  HeapFactory& HeapFactoryRef() {
    return *heap_factory_;
  }
  const HeapFactory& HeapFactoryRef() const {
    return *heap_factory_;
  }

 private:
  HeapFactory* heap_factory_;

  bool verbose_;
};

static_assert(TracefileAllocator<MallocRunner>);

}  // namespace bench

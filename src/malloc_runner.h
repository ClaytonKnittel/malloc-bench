#pragma once

#include <cstddef>
#include <optional>

#include "src/tracefile_executor.h"

namespace bench {

class MallocRunner : protected TracefileExecutor {
 public:
  static constexpr char kFailedTestPrefix[] = "[Failed]";

  MallocRunner(TracefileReader& reader, HeapFactory& heap_factory,
               bool verbose = false);

  virtual absl::Status PostAlloc(void* ptr, size_t size,
                                 std::optional<size_t> alignment,
                                 bool is_calloc) = 0;

  virtual absl::Status PreRealloc(void* ptr, size_t size) = 0;

  virtual absl::Status PostRealloc(void* new_ptr, void* old_ptr,
                                   size_t size) = 0;

  virtual absl::Status PreRelease(void* ptr) = 0;

 protected:
  void InitializeHeap(HeapFactory& heap_factory) final;
  absl::StatusOr<void*> Malloc(size_t size,
                               std::optional<size_t> alignment) final;
  absl::StatusOr<void*> Calloc(size_t nmemb, size_t size) final;
  absl::StatusOr<void*> Realloc(void* ptr, size_t size) final;
  absl::Status Free(void* ptr, std::optional<size_t> size_hint,
                    std::optional<size_t> alignment_hint) final;

 private:
  bool verbose_;
};

}  // namespace bench

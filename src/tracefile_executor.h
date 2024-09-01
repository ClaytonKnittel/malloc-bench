#pragma once

#include <cstddef>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"

#include "src/heap_factory.h"
#include "src/tracefile_reader.h"

namespace bench {

class TracefileExecutor {
 public:
  TracefileExecutor(TracefileReader&& reader, HeapFactory& heap_factory);

  absl::Status Run();

  virtual void InitializeHeap(HeapFactory& heap_factory) = 0;
  virtual absl::StatusOr<void*> Malloc(size_t size) = 0;
  virtual absl::StatusOr<void*> Calloc(size_t nmemb, size_t size) = 0;
  virtual absl::StatusOr<void*> Realloc(void* ptr, size_t size) = 0;
  virtual absl::Status Free(void* ptr) = 0;

 private:
  using IdMap = absl::flat_hash_map<void*, void*>;

  absl::Status ProcessTracefile();

  TracefileReader reader_;

  HeapFactory* const heap_factory_;

  // A map from the pointer value ID from the trace to the actual pointer value
  // allocated by the allocator.
  IdMap id_map_;
};

}  // namespace bench

#pragma once

#include <cstddef>
#include <optional>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

#include "src/heap_factory.h"
#include "src/tracefile_reader.h"

namespace bench {

class TracefileExecutor {
 public:
  TracefileExecutor(TracefileReader& reader, HeapFactory& heap_factory);

  absl::Status Run();

  virtual void InitializeHeap(HeapFactory& heap_factory) = 0;
  virtual absl::StatusOr<void*> Malloc(size_t size,
                                       std::optional<size_t> alignment) = 0;
  virtual absl::StatusOr<void*> Calloc(size_t nmemb, size_t size) = 0;
  virtual absl::StatusOr<void*> Realloc(void* ptr, size_t size) = 0;
  virtual absl::Status Free(void* ptr, std::optional<size_t> size_hint,
                            std::optional<size_t> alignment_hint) = 0;

 private:
  absl::Status ProcessTracefile();

  TracefileReader& reader_;

  HeapFactory* const heap_factory_;
};

}  // namespace bench

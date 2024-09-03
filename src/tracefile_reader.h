#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "absl/status/statusor.h"

namespace bench {

struct TraceLine {
  enum class Op {
    kMalloc,
    kCalloc,
    kRealloc,
    kFree,
  };

  Op op;

  // For free/realloc, the input pointer.
  void* input_ptr;
  // For malloc/calloc/realloc/free_hint, the requested size.
  size_t input_size;
  // For calloc, the nmemb argument.
  size_t nmemb;
  // For malloc/calloc/realloc, the returned pointer.
  void* result;
  // Process ID.
  int32_t pid;
};

class TracefileReader {
 public:
  using const_iterator = std::vector<TraceLine>::const_iterator;

  static absl::StatusOr<TracefileReader> Open(const std::string& filename);

  size_t size() const;

  const_iterator begin() const;
  const_iterator end() const;

 private:
  explicit TracefileReader(std::vector<TraceLine>&& lines);

  const std::vector<TraceLine> lines_;
};

}  // namespace bench

#pragma once

#include <fstream>
#include <optional>
#include <string>

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
  // For malloc/calloc/realloc, the requested size.
  size_t input_size;
  // For malloc/calloc/realloc, the returned pointer.
  void* result;
};

class TracefileReader {
 public:
  static absl::StatusOr<TracefileReader> Open(const std::string& filename);

  std::optional<TraceLine> NextLine();

 private:
  explicit TracefileReader(std::ifstream&& file);

  std::ifstream file_;
};

}  // namespace bench

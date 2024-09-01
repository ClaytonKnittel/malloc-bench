#pragma once

#include <fstream>
#include <optional>
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
  void* input_ptr = nullptr;
  // For malloc/calloc/realloc/free_hint, the requested size.
  size_t input_size = 0;
  // For calloc, the nmemb argument.
  size_t nmemb = 0;
  // For malloc/calloc/realloc, the returned pointer.
  void* result = nullptr;
  // Process ID.
  int32_t pid = 0;
};

class TracefileReader {
 public:
  static absl::StatusOr<TracefileReader> Open(const std::string& filename);

  absl::StatusOr<std::optional<TraceLine>> NextLine();

  // Reads all lines in a file into a vector.
  absl::StatusOr<std::vector<TraceLine>> CollectLines();

 private:
  explicit TracefileReader(std::ifstream&& file);

  std::ifstream file_;
};

}  // namespace bench

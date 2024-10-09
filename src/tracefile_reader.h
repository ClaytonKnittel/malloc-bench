#pragma once

<<<<<<< HEAD
#include <cstddef>
=======
#include <fstream>
#include <optional>
>>>>>>> d3b973fd6e938786ae4ec0560b204de2d3ba8e58
#include <string>
#include <vector>

#include "absl/status/statusor.h"

<<<<<<< HEAD
#include "proto/tracefile.pb.h"

namespace bench {

using proto::Tracefile;
using proto::TraceLine;

class TracefileReader {
 public:
  using const_iterator = google::protobuf::internal::RepeatedPtrIterator<
      const TraceLine>::iterator;

  static absl::StatusOr<TracefileReader> Open(const std::string& filename);

  size_t size() const;

  const_iterator begin() const;
  const_iterator end() const;

  const Tracefile& Tracefile() const {
    return tracefile_;
  }
=======
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
};

class TracefileReader {
 public:
  static absl::StatusOr<TracefileReader> Open(const std::string& filename);

  absl::StatusOr<std::optional<TraceLine>> NextLine();
>>>>>>> d3b973fd6e938786ae4ec0560b204de2d3ba8e58

  // Reads all lines in a file into a vector.
  absl::StatusOr<std::vector<TraceLine>> CollectLines();

 private:
<<<<<<< HEAD
  explicit TracefileReader(class Tracefile&& tracefile);

  const class Tracefile tracefile_;
=======
  explicit TracefileReader(std::ifstream&& file);

  std::ifstream file_;
>>>>>>> d3b973fd6e938786ae4ec0560b204de2d3ba8e58
};

}  // namespace bench

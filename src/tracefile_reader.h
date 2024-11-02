#pragma once

#include <cstddef>
#include <string>

#include "absl/status/statusor.h"

#include "proto/tracefile.pb.h"

namespace bench {

using proto::Tracefile;
using proto::TraceLine;

class TracefileReader {
 public:
  using const_iterator = google::protobuf::internal::RepeatedPtrIterator<
      const TraceLine>::iterator;

  TracefileReader(const TracefileReader&) = delete;
  TracefileReader(TracefileReader&&) = default;
  TracefileReader& operator=(const TracefileReader&) = delete;
  TracefileReader& operator=(TracefileReader&&) = default;

  static absl::StatusOr<TracefileReader> Open(const std::string& filename);

  size_t size() const;

  const_iterator begin() const;
  const_iterator end() const;

  const Tracefile& Tracefile() const {
    return tracefile_;
  }

 private:
  explicit TracefileReader(class Tracefile&& tracefile);

  class Tracefile tracefile_;
};

}  // namespace bench

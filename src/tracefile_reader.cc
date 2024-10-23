#include "src/tracefile_reader.h"

#include <cstddef>
#include <fstream>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"

#include "src/util.h"

namespace bench {

/* static */
absl::StatusOr<TracefileReader> TracefileReader::Open(
    const std::string& filename) {
  std::ifstream file(filename);

  if (!file.is_open()) {
    return absl::InternalError(absl::StrCat("Failed to open file ", filename));
  }

  class Tracefile tracefile;
  if (!tracefile.ParseFromIstream(&file)) {
    return absl::InternalError(
        absl::StrCat("Failed to parse ", filename, " as proto"));
  }
  file.close();

  return TracefileReader(std::move(tracefile));
}

size_t TracefileReader::size() const {
  return tracefile_.lines_size();
}

TracefileReader::const_iterator TracefileReader::begin() const {
  return tracefile_.lines().cbegin();
}

TracefileReader::const_iterator TracefileReader::end() const {
  return tracefile_.lines().cend();
}

TracefileReader::TracefileReader(class Tracefile&& tracefile)
    : tracefile_(std::move(tracefile)) {}

}  // namespace bench

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <ios>
#include <iostream>
#include <ostream>
#include <set>
#include <sstream>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/status/status.h"
#include "util/absl_util.h"

#include "src/tracefile_reader.h"

ABSL_FLAG(std::string, input, "", "File path of the trace to clean.");

namespace bench {

std::string FormatPtr(void* ptr) {
  std::ostringstream ss;
  ss << "0x" << std::uppercase << std::hex << reinterpret_cast<uintptr_t>(ptr);
  return ss.str();
}

std::string FormatLine(const TraceLine& line) {
  std::ostringstream ss;
  ss << "--" << line.pid << "-- ";
  switch (line.op) {
    case bench::TraceLine::Op::kMalloc:
      ss << "malloc(" << line.input_size << ") = " << FormatPtr(line.result);
      break;
    case bench::TraceLine::Op::kCalloc:
      ss << "calloc(" << line.input_size << "," << line.nmemb
         << ") = " << FormatPtr(line.result);
      break;
    case bench::TraceLine::Op::kRealloc:
      ss << "realloc(" << FormatPtr(line.input_ptr) << "," << line.input_size
         << ") = " << FormatPtr(line.result);
      break;
    case bench::TraceLine::Op::kFree:
      ss << "free(" << FormatPtr(line.input_ptr) << ")";
      break;
  }
  return ss.str();
}

class AllocationState {
 public:
  std::vector<void*> UnfreedPtrs() {
    std::vector<void*> ptrs;
    ptrs.reserve(allocated_ptrs_.size());
    for (void* ptr : allocated_ptrs_) {
      ptrs.push_back(ptr);
    }
    return ptrs;
  }

  /**
   * Tries to run the trace line.
   *
   * Returns whether the trace line could run successfully, else false.
   */
  bool Try(const TraceLine& line) {
    if (line.op == TraceLine::Op::kFree) {
      return Free(line.input_ptr);
    }

    if (line.op == TraceLine::Op::kCalloc ||
        line.op == TraceLine::Op::kMalloc) {
      return Malloc(line.input_size, line.result);
    }

    if (line.op == TraceLine::Op::kRealloc) {
      return Realloc(line.input_ptr, line.input_size, line.result);
    }

    return true;
  }

 private:
  bool Free(void* ptr) {
    if (ptr == nullptr) {
      return false;
    }
    if (!allocated_ptrs_.contains(ptr)) {
      return false;
    }
    allocated_ptrs_.erase(ptr);
    return true;
  }

  bool Malloc(size_t size, void* result) {
    // Don't malloc(0). Who the heck does that.
    if (size == 0) {
      return false;
    }
    if (allocated_ptrs_.contains(result)) {
      return false;
    }
    allocated_ptrs_.insert(result);
    return true;
  }

  bool Realloc(void* ptr, size_t size, void* result) {
    return (ptr == nullptr || Free(ptr)) && Malloc(size, result);
  }

  std::set<void*> allocated_ptrs_;
};

absl::Status CleanTracefile(absl::string_view input_path, std::ostream& out) {
  DEFINE_OR_RETURN(TracefileReader, reader,
                   TracefileReader::Open(std::string(input_path)));
  std::optional<int32_t> pid;
  AllocationState allocation_state;
  while (true) {
    DEFINE_OR_RETURN(std::optional<TraceLine>, line, reader.NextLine());
    if (!line.has_value()) {
      break;
    }

    if (!pid.has_value()) {
      pid = line->pid;
    }

    if (line->pid != pid) {
      continue;
    }

    if (!allocation_state.Try(*line)) {
      continue;
    }

    out << FormatLine(*line) << std::endl;
  }

  // Free all unfreed memory.
  for (void* ptr : allocation_state.UnfreedPtrs()) {
    TraceLine line = {
      .op = TraceLine::Op::kFree,
      .input_ptr = ptr,
      .pid = *pid,
    };

    out << FormatLine(line) << std::endl;
  }

  return absl::OkStatus();
}

}  // namespace bench

int main(int argc, char* argv[]) {
  absl::ParseCommandLine(argc, argv);

  const std::string& input_tracefile_path = absl::GetFlag(FLAGS_input);
  if (input_tracefile_path.empty()) {
    std::cerr << "Flag --input is required" << std::endl;
  }

  absl::Status s = bench::CleanTracefile(input_tracefile_path, std::cout);
  if (!s.ok()) {
    std::cerr << "Fatal error: " << s << std::endl;
  }

  return 0;
}

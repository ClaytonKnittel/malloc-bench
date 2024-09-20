#include <cstdlib>
#include <fstream>
#include <ios>
#include <iostream>
#include <ostream>
#include <regex>
#include <set>
#include <sstream>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "util/absl_util.h"

#include "src/tracefile_reader.h"

ABSL_FLAG(std::string, trace, "", "File path of the trace to clean.");

namespace bench {

namespace {

const std::regex kFreeRegex(
    R"(--(\d+)-- (?:free|_ZdlPv|_ZdaPv|_ZdlPvm|_ZdaPvm)\(([0-9A-Fa-fx]+)\))");
const std::regex kMallocRegex(
    R"(--(\d+)-- (?:malloc|_Znwm|_Znam)\((\d+)\) = ([0-9A-Fa-fx]+))");
const std::regex kCallocRegex(
    R"(--(\d+)-- calloc\((\d+),(\d+)\) = ([0-9A-Fa-fx]+))");
const std::regex kReallocRegex(
    R"(--(\d+)-- realloc\(([0-9A-Fa-fx]+),(\d+)\)(?:malloc\(\d+\))? = ([0-9A-Fa-fx]+))");

}  // namespace

class DirtyTracefileReader {
 public:
  using const_iterator = std::vector<TraceLine>::const_iterator;

  static absl::StatusOr<DirtyTracefileReader> Open(
      const std::string& filename) {
    std::ifstream file(filename);

    if (!file.is_open()) {
      return absl::InternalError(
          absl::StrCat("Failed to open file ", filename));
    }

    std::vector<TraceLine> lines;
    while (true) {
      std::string line;
      if (std::getline(file, line).eof()) {
        break;
      }

      DEFINE_OR_RETURN(std::optional<TraceLine>, trace_line, MatchLine(line));
      if (trace_line.has_value()) {
        lines.push_back(*trace_line);
      }
    }

    file.close();

    return DirtyTracefileReader(std::move(lines));
  }

  size_t size() const {
    return lines_.size();
  }

  const_iterator begin() const {
    return lines_.cbegin();
  }

  const_iterator end() const {
    return lines_.cend();
  }

 private:
  explicit DirtyTracefileReader(std::vector<TraceLine>&& lines)
      : lines_(std::move(lines)) {}

  template <typename T>
  static absl::StatusOr<T> ParsePrimitive(const std::string& str,
                                          const std::string& debug_name) {
    T val;
    if (!(std::istringstream(str) >> val).eof()) {
      return absl::InternalError(
          absl::StrCat("failed to parse ", str, " as ", debug_name));
    }
    return val;
  }

  static absl::StatusOr<int32_t> ParsePid(const std::string& spid) {
    return ParsePrimitive<int32_t>(spid, "pid");
  }
  static absl::StatusOr<void*> ParsePtr(const std::string& sptr) {
    return ParsePrimitive<void*>(sptr, "pointer");
  }

  static absl::StatusOr<size_t> ParseSize(const std::string& ssize) {
    return ParsePrimitive<size_t>(ssize, "size");
  }

  static absl::StatusOr<std::optional<TraceLine>> MatchLine(
      const std::string& line) {
    TraceLine parsed;
    std::smatch match;
    if (std::regex_match(line, match, kFreeRegex)) {
      parsed.op = TraceLine::Op::kFree;
      ASSIGN_OR_RETURN(parsed.pid, ParsePid(match[1].str()));
      ASSIGN_OR_RETURN(parsed.input_ptr, ParsePtr(match[2].str()));
      return parsed;
    }

    if (std::regex_match(line, match, kMallocRegex)) {
      parsed.op = TraceLine::Op::kMalloc;
      ASSIGN_OR_RETURN(parsed.pid, ParsePid(match[1].str()));
      ASSIGN_OR_RETURN(parsed.input_size, ParseSize(match[2].str()));
      ASSIGN_OR_RETURN(parsed.result, ParsePtr(match[3].str()));
      return parsed;
    }

    if (std::regex_match(line, match, kCallocRegex)) {
      parsed.op = TraceLine::Op::kCalloc;
      ASSIGN_OR_RETURN(parsed.pid, ParsePid(match[1].str()));
      ASSIGN_OR_RETURN(parsed.input_size, ParseSize(match[2].str()));
      ASSIGN_OR_RETURN(parsed.nmemb, ParseSize(match[3].str()));
      ASSIGN_OR_RETURN(parsed.result, ParsePtr(match[4].str()));
      return parsed;
    }

    if (std::regex_match(line, match, kReallocRegex)) {
      parsed.op = TraceLine::Op::kRealloc;
      ASSIGN_OR_RETURN(parsed.pid, ParsePid(match[1].str()));
      ASSIGN_OR_RETURN(parsed.input_ptr, ParsePtr(match[2].str()));
      ASSIGN_OR_RETURN(parsed.input_size, ParseSize(match[3].str()));
      ASSIGN_OR_RETURN(parsed.result, ParsePtr(match[4].str()));
      return parsed;
    }

    return std::nullopt;
  }

  const std::vector<TraceLine> lines_;
};

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
      ss << "calloc(" << line.nmemb << "," << line.input_size
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
  DEFINE_OR_RETURN(DirtyTracefileReader, reader,
                   DirtyTracefileReader::Open(std::string(input_path)));
  std::optional<int32_t> pid;
  AllocationState allocation_state;
  for (TraceLine line : reader) {
    if (!pid.has_value()) {
      pid = line.pid;
    }

    if (line.pid != pid) {
      continue;
    }

    if (!allocation_state.Try(line)) {
      continue;
    }

    out << FormatLine(line) << std::endl;
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

  const std::string& input_tracefile_path = absl::GetFlag(FLAGS_trace);
  if (input_tracefile_path.empty()) {
    std::cerr << "Flag --input is required" << std::endl;
  }

  absl::Status s = bench::CleanTracefile(input_tracefile_path, std::cout);
  if (!s.ok()) {
    std::cerr << "Fatal error: " << s << std::endl;
  }

  return 0;
}

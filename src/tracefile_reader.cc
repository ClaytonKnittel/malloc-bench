#include "src/tracefile_reader.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "util/absl_util.h"

namespace bench {

namespace {

absl::StatusOr<size_t> ParseSize(absl::string_view size) {
  size_t result;
  if (!absl::SimpleAtoi(size, &result)) {
    return absl::InvalidArgumentError(
        absl::StrCat("failed to parse ", size, " as unsigned integer"));
  }
  return result;
}

absl::StatusOr<int32_t> ParsePid(absl::string_view spid) {
  if (spid.size() < 5) {
    return absl::InvalidArgumentError(
        absl::StrCat("failed to parse ", spid, " as --<pid>--"));
  }
  spid.remove_prefix(2);
  spid.remove_suffix(2);
  return ParseSize(spid);
}

absl::StatusOr<void*> ParsePtr(absl::string_view sptr) {
  uintptr_t result;
  if (!absl::SimpleHexAtoi(sptr, &result)) {
    return absl::InvalidArgumentError(
        absl::StrCat("failed to parse ", sptr, " as a pointer"));
  }
  return reinterpret_cast<void*>(result);  // NOLINT(performance-no-int-to-ptr)
}

/**
 * Matches a single line of valigrind --trace-malloc=yes output.
 *
 * Lines have one of the following formats:
 *
 * --{pid}-- free({ptr})
 * --{pid}-- malloc({size}) = {ptr}
 * --{pid}-- calloc({size},{nmemb}) = {ptr}
 * --{pid}-- realloc({ptr},{size}) = {ptr}
 * --{pid}-- realloc(0x0,{size})malloc({size}) = {ptr}
 *
 * Where pid, size, and nmemb are decimal numbers, and ptr is a hex value.
 *
 * "free" has aliases "_ZdlPv", "_ZdaPv", "_ZdlPvm", "_ZdaPvm", and
 * "malloc" has aliases "_Znwm", "_Znam".
 */
absl::StatusOr<TraceLine> MatchLine(const std::string& line) {
  TraceLine parsed;
  std::smatch match;

  auto x = absl::StrSplit(line, ' ');
  auto it = x.begin();
  if (it == x.end()) {
    return absl::InvalidArgumentError(
        absl::StrCat("failed to parse ", line, " as operation"));
  }
  ASSIGN_OR_RETURN(parsed.pid, ParsePid(*it));

  if (++it == x.end()) {
    return absl::InvalidArgumentError(
        absl::StrCat("failed to parse ", line, " as operation"));
  }
  absl::string_view op = *it;
  if (op.starts_with("free") || op.starts_with("_ZdlPv") ||
      op.starts_with("_ZdaPv") || op.starts_with("_ZdlPvm") ||
      op.starts_with("_ZdaPvm")) {
    parsed.op = TraceLine::Op::kFree;
    if (op.size() < 7) {
      return absl::InvalidArgumentError(
          absl::StrCat("failed to parse ", op, " as free(<ptr>)"));
    }

    size_t prefix_len;
    if (op.starts_with("_Zd")) {
      if (op[6] == 'm') {
        prefix_len = 8;
      } else {
        prefix_len = 7;
      }
    } else {
      prefix_len = 5;
    }
    op.remove_prefix(prefix_len);
    op.remove_suffix(1);
    ASSIGN_OR_RETURN(parsed.input_ptr, ParsePtr(op));
  } else if (op.starts_with("malloc") || op.starts_with("_Znwm") ||
             op.starts_with("_Znam")) {
    parsed.op = TraceLine::Op::kMalloc;
    if (op.size() < 8) {
      return absl::InvalidArgumentError(
          absl::StrCat("failed to parse ", op, " as malloc(<size>)"));
    }
    op.remove_prefix(op[0] == 'm' ? 7 : 6);
    op.remove_suffix(1);
    ASSIGN_OR_RETURN(parsed.input_size, ParseSize(op));
  } else if (op.starts_with("calloc")) {
    parsed.op = TraceLine::Op::kCalloc;
    if (op.size() < 11) {
      return absl::InvalidArgumentError(
          absl::StrCat("failed to parse ", op, " as calloc(<nmemb>,<size>)"));
    }
    op.remove_prefix(7);
    op.remove_suffix(1);
    size_t res = op.find_first_of(',');
    if (res == absl::string_view::npos) {
      return absl::InvalidArgumentError(
          absl::StrCat("failed to parse ", op, " as calloc(<nmemb>,<size>)"));
    }
    ASSIGN_OR_RETURN(parsed.nmemb, ParseSize(op.substr(0, res)));
    ASSIGN_OR_RETURN(parsed.input_size, ParseSize(op.substr(res + 1)));
  } else if (op.starts_with("realloc")) {
    parsed.op = TraceLine::Op::kRealloc;
    if (op.size() < 12) {
      return absl::InvalidArgumentError(
          absl::StrCat("failed to parse ", op, " as realloc(<ptr>,<size>)"));
    }
    op.remove_prefix(8);
    op.remove_suffix(1);
    size_t res = op.find_first_of(',');
    if (res == absl::string_view::npos) {
      return absl::InvalidArgumentError(
          absl::StrCat("failed to parse ", op, " as realloc(<ptr>,<size>)"));
    }
    ASSIGN_OR_RETURN(parsed.input_ptr, ParsePtr(op.substr(0, res)));

    if (parsed.input_ptr == nullptr) {
      size_t res2 = op.find_first_of(')', res + 1);
      if (res2 == absl::string_view::npos) {
        res2 = op.size();
      }
      ASSIGN_OR_RETURN(parsed.input_size,
                       ParseSize(op.substr(res + 1, res2 - (res + 1))));
    } else {
      ASSIGN_OR_RETURN(parsed.input_size, ParseSize(op.substr(res + 1)));
    }
  } else {
    return absl::InvalidArgumentError(
        absl::StrCat("failed to parse ", line, " as operation"));
  }

  if (parsed.op != TraceLine::Op::kFree) {
    if (++it == x.end() || ++it == x.end()) {
      return absl::InvalidArgumentError(
          absl::StrCat("failed to parse ", line, " as operation"));
    }
    ASSIGN_OR_RETURN(parsed.result, ParsePtr(*it));
  }

  return parsed;
}

}  // namespace

/* static */
absl::StatusOr<TracefileReader> TracefileReader::Open(
    const std::string& filename) {
  std::ifstream file(filename);

  if (!file.is_open()) {
    return absl::InternalError(absl::StrCat("Failed to open file ", filename));
  }

  std::vector<TraceLine> lines;
  while (true) {
    std::string line;
    if (std::getline(file, line).eof()) {
      break;
    }

    DEFINE_OR_RETURN(TraceLine, trace_line, MatchLine(line));
    lines.push_back(trace_line);
  }

  file.close();

  return TracefileReader(std::move(lines));
}

size_t TracefileReader::size() const {
  return lines_.size();
}

TracefileReader::const_iterator TracefileReader::begin() const {
  return lines_.cbegin();
}

TracefileReader::const_iterator TracefileReader::end() const {
  return lines_.cend();
}

TracefileReader::TracefileReader(std::vector<TraceLine>&& lines)
    : lines_(std::move(lines)) {}

}  // namespace bench

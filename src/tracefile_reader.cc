#include "src/tracefile_reader.h"

#include <fstream>
#include <iostream>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "util/absl_util.h"

namespace bench {

namespace {

template <typename T>
absl::StatusOr<T> ParsePrimitive(const std::string& str,
                                 const std::string& debug_name) {
  T val;
  if (!(std::istringstream(str) >> val).eof()) {
    return absl::InternalError(
        absl::StrCat("failed to parse ", str, " as ", debug_name));
  }
  return val;
}

absl::StatusOr<int32_t> ParsePid(const std::string& spid) {
  return ParsePrimitive<int32_t>(spid, "pid");
}

absl::StatusOr<void*> ParsePtr(const std::string& sptr) {
  return ParsePrimitive<void*>(sptr, "pointer");
}

absl::StatusOr<size_t> ParseSize(const std::string& ssize) {
  return ParsePrimitive<size_t>(ssize, "size");
}

const std::regex kFreeRegex(
    R"(--(\d+)-- (?:free|_ZdlPv|_ZdaPv|_ZdlPvm|_ZdaPvm)\(([0-9A-Fa-fx]+)\))");
const std::regex kMallocRegex(
    R"(--(\d+)-- (?:malloc|_Znwm|_Znam)\((\d+)\) = ([0-9A-Fa-fx]+))");
const std::regex kCallocRegex(
    R"(--(\d+)-- calloc\((\d+),(\d+)\) = ([0-9A-Fa-fx]+))");
const std::regex kReallocRegex(
    R"(--(\d+)-- realloc\(([0-9A-Fa-fx]+),(\d+)\)(?:malloc\(\d+\))? = ([0-9A-Fa-fx]+))");

/**
 * Matches a single line of valigrind --trace-malloc=yes output.
 *
 * Lines have one of the following formats:
 *
 * --{pid}-- free({ptr})
 * --{pid}-- malloc({size}) = {ptr}
 * --{pid}-- calloc({size},{nmemb}) = {ptr}
 * --{pid}-- realloc({ptr},{size}) = {ptr}
 * --{pid}-- realloc(0x0, {size})malloc({size}) = {ptr}
 *
 * Where pid, size, and nmemb are decimal numbers, and ptr is a hex value.
 *
 * "free" has aliases "_ZdlPv", "_ZdaPv", "_ZdlPvm", "_ZdaPvm", and
 * "malloc" has aliases "_Znwm", "_Znam".
 */
absl::StatusOr<std::optional<TraceLine>> MatchLine(const std::string& line) {
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

}  // namespace

/* static */
absl::StatusOr<TracefileReader> TracefileReader::Open(
    const std::string& filename) {
  std::ifstream file(filename);

  if (!file.is_open()) {
    return absl::InternalError(absl::StrCat("Failed to open file ", filename));
  }

  return TracefileReader(std::move(file));
}

absl::StatusOr<absl::Span<const TraceLine>> TracefileReader::CollectLines() {
  while (true) {
    DEFINE_OR_RETURN(std::optional<TraceLine>, line, NextLine());
    if (!line.has_value()) {
      break;
    }
  }

  return lines_;
}

TracefileReader::TracefileReader(std::ifstream&& file)
    : file_(std::move(file)) {}

absl::StatusOr<std::optional<TraceLine>> TracefileReader::NextLine() {
  while (true) {
    std::string line;
    if (std::getline(file_, line).eof()) {
      return std::nullopt;
    }

    DEFINE_OR_RETURN(std::optional<TraceLine>, trace_line, MatchLine(line));
    if (trace_line.has_value()) {
      lines_.push_back(*trace_line);
      return *trace_line;
    }
  }
}

}  // namespace bench

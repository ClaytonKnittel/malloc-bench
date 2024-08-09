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
#include "util/absl_util.h"

namespace bench {

static const std::regex kLineRegex(
    R"(--\d+-- ([a-zA-Z_]+)\(([0-9A-Fx]+)(,\d+)?\)( = ([0-9A-Fx]+))?)");

/* static */
absl::StatusOr<TracefileReader> TracefileReader::Open(
    const std::string& filename) {
  std::ifstream file(filename);

  if (!file.is_open()) {
    return absl::InternalError(absl::StrCat("Failed to open file ", filename));
  }

  return TracefileReader(std::move(file));
}

absl::StatusOr<std::optional<TraceLine>> TracefileReader::NextLine() {
  while (true) {
    std::string line;
    if (std::getline(file_, line).eof()) {
      return std::nullopt;
    }

    std::smatch match;
    if (!std::regex_match(line, match, kLineRegex)) {
      continue;
    }

    const std::string method = match[1].str();
    const std::string sarg1 = match[2].str();
    const std::string sarg2 = match[3].str();
    const std::string sres = match[5].str();

    if (method == "malloc" || method == "_Znwm" || method == "_Znam") {
      size_t arg1;
      void* res;
      if (!(std::istringstream(sarg1) >> arg1).eof()) {
        return absl::InternalError(
            absl::StrCat("Failed to parse ", sarg1, " as integer for malloc"));
      }
      if (!(std::istringstream(sres) >> res).eof()) {
        return absl::InternalError(
            absl::StrCat("Failed to parse ", sres, " as pointer for malloc"));
      }

      return TraceLine{
        .op = TraceLine::Op::kMalloc,
        .input_size = arg1,
        .result = res,
      };
    }
    if (method == "calloc") {
      size_t arg1;
      size_t arg2;
      void* res;
      if (!(std::istringstream(sarg1) >> arg1).eof()) {
        return absl::InternalError(
            absl::StrCat("Failed to parse ", sarg1, " as integer for calloc"));
      }
      if (!(std::istringstream(sarg2.substr(1)) >> arg2).eof()) {
        return absl::InternalError(absl::StrCat(
            "Failed to parse ", sarg2.substr(1), " as integer for calloc"));
      }
      if (!(std::istringstream(sres) >> res).eof()) {
        return absl::InternalError(
            absl::StrCat("Failed to parse ", sres, " as pointer for calloc"));
      }

      return TraceLine{
        .op = TraceLine::Op::kCalloc,
        .input_size = arg2,
        .nmemb = arg1,
        .result = res,
      };
    }
    if (method == "realloc") {
      void* arg1;
      size_t arg2;
      void* res;
      if (!(std::istringstream(sarg1) >> arg1).eof()) {
        return absl::InternalError(
            absl::StrCat("Failed to parse ", sarg1, " as pointer for realloc"));
      }
      if (!(std::istringstream(sarg2.substr(1)) >> arg2).eof()) {
        return absl::InternalError(absl::StrCat(
            "Failed to parse ", sarg2.substr(1), " as integer for realloc"));
      }
      if (!(std::istringstream(sres) >> res).eof()) {
        return absl::InternalError(
            absl::StrCat("Failed to parse ", sres, " as pointer for realloc"));
      }

      return TraceLine{
        .op = TraceLine::Op::kRealloc,
        .input_ptr = arg1,
        .input_size = arg2,
        .result = res,
      };
    }
    if (method == "free" || method == "_ZdlPv" || method == "_ZdaPv" ||
        method == "_ZdlPvm" || method == "_ZdaPvm") {
      void* arg1;
      if (!(std::istringstream(sarg1) >> arg1).eof()) {
        return absl::InternalError(
            absl::StrCat("Failed to parse ", sarg1, " as pointer for free"));
      }

      return TraceLine{
        .op = TraceLine::Op::kFree,
        .input_ptr = arg1,
      };
    }
  }
}

absl::StatusOr<std::vector<TraceLine>> TracefileReader::CollectLines() {
  std::vector<TraceLine> lines;

  while (true) {
    DEFINE_OR_RETURN(std::optional<TraceLine>, line, NextLine());
    if (!line.has_value()) {
      break;
    }

    lines.push_back(line.value());
  }

  return lines;
}

TracefileReader::TracefileReader(std::ifstream&& file)
    : file_(std::move(file)) {}

}  // namespace bench

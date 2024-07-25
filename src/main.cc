#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <vector>

#include "absl/status/status.h"
#include "absl/time/time.h"

#include "src/correctness_checker.h"
#include "src/perftest.h"
#include "src/tracefile_reader.h"
#include "src/util.h"
#include "src/utiltest.h"

namespace bench {

struct TraceResult {
  std::string trace;
  bool correct;
  absl::Duration time_per_op;
  double utilization;
};

absl::StatusOr<TraceResult> RunTrace(const std::string& tracefile) {
  TraceResult result{
    .trace = tracefile,
  };

  // Check for correctness.
  absl::Status correctness_status = CorrectnessChecker::Check(tracefile);
  if (correctness_status.ok()) {
    result.correct = true;
  } else {
    if (!CorrectnessChecker::IsFailedTestStatus(correctness_status)) {
      return correctness_status;
    }
    result.correct = false;
  }

  if (result.correct) {
    DEFINE_OR_RETURN(absl::Duration, time_per_op, TimeTrace(tracefile));
    DEFINE_OR_RETURN(double, utilization, MeasureUtilization(tracefile));

    result.time_per_op = time_per_op;
    result.utilization = utilization;
  }

  return result;
}

void PrintTestResults(const std::vector<TraceResult>& results) {
  size_t max_file_len = 0;
  for (const TraceResult& result : results) {
    max_file_len = std::max(result.trace.size(), max_file_len);
  }

  std::cout << "-" << std::setw(max_file_len) << std::setfill('-') << ""
            << std::setfill(' ') << "------------------------------------------"
            << std::endl;
  std::cout << "| trace" << std::setw(max_file_len - 5) << ""
            << " | correct? | avg time/op | utilization |" << std::endl;
  std::cout << "-" << std::setw(max_file_len) << std::setfill('-') << ""
            << std::setfill(' ') << "------------------------------------------"
            << std::endl;
  for (const TraceResult& result : results) {
    std::cout << "| " << std::setw(max_file_len) << std::left << result.trace
              << " |        " << (result.correct ? "Y" : "N") << " | ";
    if (result.correct) {
      std::cout << std::setw(11) << result.time_per_op << " | " << std::setw(11)
                << result.utilization << " |" << std::endl;
    } else {
      std::cout << "            |             |" << std::endl;
    }
  }
  std::cout << "-" << std::setw(max_file_len) << std::setfill('-') << ""
            << std::setfill(' ') << "------------------------------------------"
            << std::endl;

  std::cout << std::endl << "Summary:" << std::endl;
}

absl::Status PrintTrace(const std::string& tracefile) {
  DEFINE_OR_RETURN(TracefileReader, reader, TracefileReader::Open(tracefile));

  while (true) {
    DEFINE_OR_RETURN(std::optional<TraceLine>, line, reader.NextLine());
    if (line.has_value()) {
      break;
    }

    switch (line->op) {
      case TraceLine::Op::kMalloc:
        std::cout << "malloc(" << line->input_size << ") = " << line->result
                  << std::endl;
        break;
      case TraceLine::Op::kCalloc:
        std::cout << "calloc(" << line->nmemb << ", " << line->input_size
                  << ") = " << line->result << std::endl;
        break;
      case TraceLine::Op::kRealloc:
        std::cout << "realloc(" << line->input_ptr << ", " << line->input_size
                  << ") = " << line->result << std::endl;
        break;
      case TraceLine::Op::kFree:
        if (line->input_ptr != nullptr) {
          std::cout << "free(" << line->input_ptr << ")" << std::endl;
        }
        break;
    }
  }

  return absl::OkStatus();
}

}  // namespace bench

int main() {
  // return PrintTrace("traces/onoro-cc.trace");

  std::vector<bench::TraceResult> results;

  for (const auto& tracefile : {
           "traces/simple.trace",
           "traces/simple_calloc.trace",
           "traces/simple_realloc.trace",
       }) {
    auto result = bench::RunTrace(tracefile);
    if (!result.ok()) {
      std::cerr << "Failed to run trace " << tracefile << ": "
                << result.status() << std::endl;
      return -1;
    }

    results.push_back(result.value());
  }
  PrintTestResults(results);

  return 0;
}

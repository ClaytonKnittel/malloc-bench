#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <ios>
#include <iostream>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/strip.h"
#include "util/absl_util.h"

#include "src/correctness_checker.h"
#include "src/heap_factory.h"
#include "src/mmap_heap_factory.h"
#include "src/perfetto.h"
#include "src/perftest.h"
#include "src/tracefile_executor.h"
#include "src/tracefile_reader.h"
#include "src/utiltest.h"

ABSL_FLAG(std::string, trace, "",
          "If set, a specific tracefile to run (must start with \"traces/\").");

ABSL_FLAG(bool, skip_correctness, false,
          "If true, correctness checking is skipped.");

ABSL_FLAG(bool, ignore_test, false, "If true, test traces are not run.");

ABSL_FLAG(bool, ignore_hard, true,
          "If true, \"hard traces\" are skipped (i.e. ones that call memalign, "
          "or use a lot of memory).");

ABSL_FLAG(size_t, perftest_iters, 1000000,
          "The minimum number of alloc/free operations to perform for each "
          "tracefile when measuring allocator throughput.");

ABSL_FLAG(uint32_t, threads, 1,
          "If not 1, the number of threads to run all tests with.");

namespace bench {

struct TraceResult {
  std::string trace;
  bool correct;
  double mega_ops;
  double utilization;
};

bool ShouldIgnoreForScoring(const std::string& trace) {
  return absl::StrContains(trace, "simple") ||
         absl::StrContains(trace, "test") ||
         absl::StrContains(trace, "/bdd-") ||
         absl::StrContains(trace, "/cbit-") ||
         absl::StrContains(trace, "/syn-") ||
         absl::StrContains(trace, "/ngram-") ||
         absl::StrContains(trace, "/server.trace");
}

bool IsHard(const std::string& trace) {
  return absl::StrContains(trace, "/gto.trace");
}

absl::StatusOr<TraceResult> RunTrace(const std::string& tracefile,
                                     HeapFactory& heap_factory) {
  TraceResult result{
    .trace = tracefile,
  };

  DEFINE_OR_RETURN(TracefileReader, reader, TracefileReader::Open(tracefile));

  TracefileExecutorOptions options = {
    .n_threads = absl::GetFlag(FLAGS_threads),
  };

  // Check for correctness.
  if (!absl::GetFlag(FLAGS_skip_correctness)) {
    absl::Status correctness_status = CorrectnessChecker::Check(
        reader, heap_factory, /*verbose=*/false, options);
    if (correctness_status.ok()) {
      result.correct = true;
    } else {
      if (!CorrectnessChecker::IsFailedTestStatus(correctness_status)) {
        return correctness_status;
      }

      std::cout << "Failed " << tracefile << ": " << correctness_status
                << std::endl;
      result.correct = false;
    }
  } else {
    result.correct = true;
  }

  if (result.correct) {
    auto perf_util_result =
        [&reader, &heap_factory,
         &options]() -> absl::StatusOr<std::pair<double, double>> {
      DEFINE_OR_RETURN(
          double, mega_ops,
          Perftest::TimeTrace(reader, heap_factory,
                              absl::GetFlag(FLAGS_perftest_iters), options));
      DEFINE_OR_RETURN(
          double, utilization,
          Utiltest::MeasureUtilization(reader, heap_factory, options));

      return std::make_pair(mega_ops, utilization);
    }();
    if (!perf_util_result.ok()) {
      std::cout << "Failed " << tracefile << ": " << perf_util_result.status()
                << std::endl;
      result.correct = false;
    } else {
      auto [mega_ops, utilization] = perf_util_result.value();
      result.mega_ops = mega_ops;
      result.utilization = utilization;
    }
  }

  return result;
}

void PrintTestResults(const std::vector<TraceResult>& results) {
  size_t max_file_len = 0;
  for (const TraceResult& result : results) {
    max_file_len = std::max(result.trace.size(), max_file_len);
  }
  uint32_t n_correct = 0;
  double total_util = 0;
  double total_mops_geom = 1;
  bool all_correct = true;

  for (const TraceResult& result : results) {
    all_correct = all_correct && result.correct;
    if (result.correct && !ShouldIgnoreForScoring(result.trace)) {
      n_correct++;
      total_util += result.utilization;
    }
  }
  for (const TraceResult& result : results) {
    if (result.correct && !ShouldIgnoreForScoring(result.trace)) {
      total_mops_geom *= pow(result.mega_ops, 1. / n_correct);
    }
  }

  std::cout << "-" << std::setw(max_file_len) << std::setfill('-') << ""
            << std::setfill(' ')
            << "-------------------------------------------" << std::endl;
  std::cout << "| trace" << std::setw(max_file_len - 5) << ""
            << " | correct? | mega ops / s | utilization |" << std::endl;
  std::cout << "-" << std::setw(max_file_len) << std::setfill('-') << ""
            << std::setfill(' ')
            << "-------------------------------------------" << std::endl;
  for (const TraceResult& result : results) {
    if (ShouldIgnoreForScoring(result.trace)) {
      std::cout << "|*";
    } else {
      std::cout << "| ";
    }

    std::cout << std::setw(max_file_len) << std::left << result.trace
              << " |        " << (result.correct ? "Y" : "N") << " | ";
    if (result.correct) {
      std::cout << std::right << std::fixed << std::setprecision(1)
                << std::setw(12) << result.mega_ops << " | " << std::setw(10)
                << (100 * result.utilization) << "% |" << std::endl;
    } else {
      std::cout << "             |             |" << std::endl;
    }
  }
  std::cout << "-" << std::setw(max_file_len) << std::setfill('-') << ""
            << std::setfill(' ')
            << "-------------------------------------------" << std::endl;
  if (!absl::GetFlag(FLAGS_ignore_test)) {
    std::cout << "* = ignored for scoring" << std::endl;
  }

  n_correct = std::max(n_correct, 1U);
  std::cout << std::endl << "Summary:" << std::endl;
  std::cout << "All correct? " << (all_correct ? "Y" : "N") << std::endl;
  std::cout << "Average utilization: " << (100 * (total_util / n_correct))
            << "%" << std::endl;
  std::cout << "Average mega ops / s: " << total_mops_geom << std::endl;

  if (all_correct) {
    constexpr double kMinUtilThresh = 0.55;
    constexpr double kMaxUtilThresh = 0.875;
    constexpr double kMinOpsThresh = 40;
    constexpr double kMaxOpsThresh = 100;

    double util_score = std::clamp((total_util / n_correct - kMinUtilThresh) /
                                       (kMaxUtilThresh - kMinUtilThresh),
                                   0., 1.);
    double ops_score =
        std::clamp((std::log(total_mops_geom) - std::log(kMinOpsThresh)) /
                       (std::log(kMaxOpsThresh) - std::log(kMinOpsThresh)),
                   0., 1.);

    double score = 0.5 * util_score + 0.5 * ops_score;
    std::cout << "Score: " << std::fixed << std::setprecision(1)
              << (score * 100) << "%" << std::endl;
  } else {
    std::cout << "Score: 0%" << std::endl;
  }
}

std::vector<std::string> ListTracefiles() {
  std::vector<std::string> paths;
  for (const auto& dir_entry : std::filesystem::directory_iterator("traces")) {
    const std::string& tracefile = dir_entry.path();
    if (!tracefile.ends_with(".trace")) {
      continue;
    }
    paths.push_back(tracefile);
  }
  std::sort(paths.begin(), paths.end());
  return paths;
}

int RunAllTraces() {
  std::vector<bench::TraceResult> results;
  MMapHeapFactory heap_factory;

  for (const auto& tracefile : ListTracefiles()) {
    if (absl::GetFlag(FLAGS_ignore_test) && ShouldIgnoreForScoring(tracefile)) {
      continue;
    }
    if (absl::GetFlag(FLAGS_ignore_hard) && IsHard(tracefile)) {
      continue;
    }

    auto result = RunTrace(tracefile, heap_factory);
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

}  // namespace bench

int main(int argc, char* argv[]) {
  absl::ParseCommandLine(argc, argv);
  bench::Perfetto perfetto;

  // Strip .gz in case the user specifies the compressed trace.
  const std::string tracefile(
      absl::StripSuffix(absl::GetFlag(FLAGS_trace), ".gz"));
  if (tracefile.empty()) {
    return bench::RunAllTraces();
  }

  bench::MMapHeapFactory heap_factory;
  auto result = bench::RunTrace(tracefile, heap_factory);
  if (!result.ok()) {
    std::cerr << "Failed to run trace " << tracefile << ": " << result.status()
              << std::endl;
    return -1;
  }

  std::cout << tracefile << std::endl;
  std::cout << "Correct? " << (result->correct ? "Y" : "N") << std::endl;
  if (result->correct) {
    std::cout << "mega-ops / s: " << std::fixed << std::setprecision(1)
              << result->mega_ops << std::endl;
    std::cout << "Utilization:  " << std::fixed << std::setprecision(1)
              << (result->utilization * 100) << "%" << std::endl;
  }
  return 0;
}

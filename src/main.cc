#include <iostream>

#include "absl/status/status.h"

#include "src/correctness_checker.h"
#include "src/tracefile_reader.h"

int PrintTrace(const std::string& tracefile) {
  auto res = bench::TracefileReader::Open(tracefile);
  if (!res.ok()) {
    std::cerr << res.status() << std::endl;
    return -1;
  }

  bench::TracefileReader& reader = res.value();
  std::optional<bench::TraceLine> line;
  while ((line = reader.NextLine()).has_value()) {
    switch (line->op) {
      case bench::TraceLine::Op::kMalloc:
        std::cout << "malloc(" << line->input_size << ") = " << line->result
                  << std::endl;
        break;
      case bench::TraceLine::Op::kCalloc:
        std::cout << "calloc(" << line->nmemb << ", " << line->input_size
                  << ") = " << line->result << std::endl;
        break;
      case bench::TraceLine::Op::kRealloc:
        std::cout << "realloc(" << line->input_ptr << ", " << line->input_size
                  << ") = " << line->result << std::endl;
        break;
      case bench::TraceLine::Op::kFree:
        if (line->input_ptr != nullptr) {
          std::cout << "free(" << line->input_ptr << ")" << std::endl;
        }
        break;
      case bench::TraceLine::Op::kFreeHint:
        if (line->input_ptr != nullptr) {
          std::cout << "free(" << line->input_ptr << ", " << line->input_size
                    << ")" << std::endl;
        }
        break;
    }
  }

  return 0;
}

int main() {
  // return PrintTrace("traces/onoro-cc.trace");

  absl::Status result =
      bench::CorrectnessChecker::Check("traces/onoro-cc.trace");
  if (!result.ok()) {
    std::cerr << "Failed: " << result << std::endl;
    return -1;
  }

  return 0;
}

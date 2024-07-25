#include <cstdlib>
#include <iostream>

#include "absl/status/status.h"

#include "src/correctness_checker.h"
#include "src/tracefile_reader.h"
#include "src/util.h"

absl::Status PrintTrace(const std::string& tracefile) {
  DEFINE_OR_RETURN(bench::TracefileReader, reader,
                   bench::TracefileReader::Open(tracefile));

  while (true) {
    DEFINE_OR_RETURN(std::optional<bench::TraceLine>, line, reader.NextLine());
    if (line.has_value()) {
      break;
    }

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
    }
  }

  return absl::OkStatus();
}

int main() {
  void* x = calloc(10, 4);
  free(x);
  // return PrintTrace("traces/onoro-cc.trace");

  // absl::Status result =
  //     bench::CorrectnessChecker::Check("traces/simple_realloc.trace");
  // if (!result.ok()) {
  //   std::cerr << "Failed: " << result << std::endl;
  //   return -1;
  // }

  return 0;
}

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
        std::cout << "calloc(" << line->input_size << ") = " << line->result
                  << std::endl;
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

  return 0;
}

int main() {
  // int* x = new int(10);
  // std::cout << "Malloc bench" << std::endl;
  // delete x;

  // void* ptr = malloc(100);
  // void* new_ptr = realloc(ptr, 150);
  // free(new_ptr == nullptr ? ptr : new_ptr);

  // return
  // PrintTrace("/home/cknittel/Documents/VSCode/malloc-bench/test.trace");

  absl::Status result = bench::CorrectnessChecker::Check(
      "/home/cknittel/Documents/VSCode/malloc-bench/test.trace");
  if (!result.ok()) {
    std::cerr << "Failed: " << result << std::endl;
    return -1;
  }

  return 0;
}

#include <string>

#include "absl/status/statusor.h"

#include "src/mmap_heap_factory.h"
#include "src/perftest.h"
#include "src/tracefile_reader.h"

int main() {
  for (const auto& tracefile : {
           "traces/four-in-a-row.trace",
           "traces/grep.trace",
           "traces/haskell-web-server.trace",
           "traces/onoro.trace",
           "traces/onoro-cc.trace",
           "traces/scp.trace",
           "traces/solitaire.trace",
           "traces/ssh.trace",
       }) {
    bench::MMapHeapFactory heap_factory;
    absl::StatusOr<bench::TracefileReader> reader =
        bench::TracefileReader::Open(tracefile);
    if (!reader.ok()) {
      std::cerr << reader.status() << std::endl;
      return -1;
    }

    auto result = bench::TimeTrace(reader.value(), heap_factory,
                                   /*min_desired_ops=*/100000000);
    if (result.ok()) {
      std::cout << tracefile << ": " << result.value() << " mega ops / s"
                << std::endl;
    } else {
      std::cout << tracefile << ": " << result.status() << std::endl;
      break;
    }
  }

  return 0;
}

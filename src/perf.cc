#include <cmath>
#include <string>

#include "absl/status/statusor.h"
#include "util/absl_util.h"

#include "src/perftest.h"

int main() {
  for (const auto& tracefile : {
           "traces/bdd-aa32.trace",
           "traces/bdd-aa4.trace",
           "traces/bdd-ma4.trace",
           "traces/bdd-nq7.trace",
           "traces/cbit-abs.trace",
           "traces/cbit-parity.trace",
           "traces/cbit-satadd.trace",
           "traces/cbit-xyz.trace",
           "traces/ngram-gulliver1.trace",
           "traces/ngram-gulliver2.trace",
           "traces/ngram-moby1.trace",
           "traces/ngram-shake1.trace",
           /*"traces/onoro.trace",*/ "traces/onoro-cc.trace",
           "traces/server.trace",
           "traces/syn-array.trace",
           "traces/syn-mix.trace",
           "traces/syn-mix-realloc.trace",
           "traces/syn-string.trace",
           "traces/syn-struct.trace",
       }) {
    auto result = bench::TimeTrace(tracefile, /*min_desired_ops=*/100000000);
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

#include "src/perftest.h"

#include <cstddef>
#include <cstdlib>

#include "absl/status/statusor.h"
#include "absl/time/time.h"

#include "src/tracefile_executor.h"
#include "src/tracefile_reader.h"

namespace bench {

/* static */
absl::StatusOr<double> Perftest::TimeTrace(
    TracefileReader& reader, uint64_t min_desired_ops,
    const TracefileExecutorOptions& options) {
  TracefileExecutor<Perftest> perftest(reader);

  const uint64_t num_repetitions = (min_desired_ops - 1) / reader.size() + 1;

  absl::Time start = absl::Now();
  RETURN_IF_ERROR(perftest.RunRepeated(num_repetitions, options));
  absl::Time end = absl::Now();

  uint64_t total_ops = num_repetitions * reader.size();
  double seconds = absl::FDivDuration((end - start), absl::Seconds(1));
  return total_ops / seconds / 1000000;
}

absl::Status Perftest::PostAlloc(void* ptr, size_t size,
                                 std::optional<size_t> alignment,
                                 bool is_calloc) {
  (void) ptr;
  (void) size;
  (void) alignment;
  (void) is_calloc;
  return absl::OkStatus();
}

absl::Status Perftest::PreRealloc(void* ptr, size_t size) {
  (void) ptr;
  (void) size;
  return absl::OkStatus();
}

absl::Status Perftest::PostRealloc(void* new_ptr, void* old_ptr, size_t size) {
  (void) new_ptr;
  (void) old_ptr;
  (void) size;
  return absl::OkStatus();
}

absl::Status Perftest::PreRelease(void* ptr) {
  (void) ptr;
  return absl::OkStatus();
}

}  // namespace bench

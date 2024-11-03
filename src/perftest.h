#pragma once

#include <cstddef>
#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

#include "src/malloc_runner.h"
#include "src/tracefile_reader.h"

namespace bench {

class Perftest : public MallocRunner<MallocRunnerConfig{ .perftest = true }> {
 public:
  Perftest() = default;

  // On success, returns the number of MOps/s (1,000,000 ops per second).
  static absl::StatusOr<double> TimeTrace(
      TracefileReader& reader, uint64_t min_desired_ops,
      const TracefileExecutorOptions& options = TracefileExecutorOptions());

  absl::Status PostAlloc(void* ptr, size_t size,
                         std::optional<size_t> alignment,
                         bool is_calloc) override;

  absl::Status PreRealloc(void* ptr, size_t size) override;

  absl::Status PostRealloc(void* new_ptr, void* old_ptr, size_t size) override;

  absl::Status PreRelease(void* ptr) override;
};

}  // namespace bench

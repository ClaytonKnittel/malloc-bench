#pragma once

#include "absl/status/statusor.h"

#include "src/heap_factory.h"

namespace bench {

// On success, returns the number of MOps/s (1000 ops per second).
absl::StatusOr<double> TimeTrace(const std::string& tracefile,
                                 HeapFactory& heap_factory,
                                 size_t min_desired_ops = 1000000);

}  // namespace bench

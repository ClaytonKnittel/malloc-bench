#pragma once

<<<<<<< HEAD
#include <cstddef>

#include "absl/status/statusor.h"

#include "src/heap_factory.h"
#include "src/tracefile_reader.h"

namespace bench {

// On success, returns the number of MOps/s (1000 ops per second).
absl::StatusOr<double> TimeTrace(TracefileReader& reader,
                                 HeapFactory& heap_factory,
                                 size_t min_desired_ops);
=======
#include "absl/status/statusor.h"

namespace bench {

// On success, returns the number of MOps/s (1000 ops per second).
absl::StatusOr<double> TimeTrace(const std::string& tracefile);
>>>>>>> d3b973fd6e938786ae4ec0560b204de2d3ba8e58

}  // namespace bench

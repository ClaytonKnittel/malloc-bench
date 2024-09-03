#pragma once

#include "absl/status/statusor.h"

#include "src/heap_factory.h"
#include "src/tracefile_reader.h"

namespace bench {

// On success, returns the number of MOps/s (1000 ops per second).
absl::StatusOr<double> TimeTrace(TracefileReader& reader,
                                 HeapFactory& heap_factory);

}  // namespace bench

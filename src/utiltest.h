#pragma once

#include "absl/status/statusor.h"

#include "src/heap_factory.h"
#include "src/tracefile_reader.h"

namespace bench {

absl::StatusOr<double> MeasureUtilization(TracefileReader& reader,
                                          HeapFactory& heap_factory);

}

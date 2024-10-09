#pragma once

#include "absl/status/statusor.h"

<<<<<<< HEAD
#include "src/heap_factory.h"
#include "src/tracefile_reader.h"

namespace bench {

absl::StatusOr<double> MeasureUtilization(TracefileReader& reader,
                                          HeapFactory& heap_factory);
=======
namespace bench {

absl::StatusOr<double> MeasureUtilization(const std::string& tracefile);
>>>>>>> d3b973fd6e938786ae4ec0560b204de2d3ba8e58

}

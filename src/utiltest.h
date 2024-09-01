#pragma once

#include "absl/status/statusor.h"

#include "src/heap_factory.h"

namespace bench {

absl::StatusOr<double> MeasureUtilization(const std::string& tracefile,
                                          HeapFactory& heap_factory);

}

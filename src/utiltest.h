#pragma once

#include "absl/status/statusor.h"

namespace bench {

absl::StatusOr<double> MeasureUtilization(const std::string& tracefile);

}

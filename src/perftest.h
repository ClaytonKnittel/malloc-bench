#pragma once

#include "absl/status/statusor.h"
#include "absl/time/time.h"

namespace bench {

absl::StatusOr<absl::Duration> TimeTrace(const std::string& tracefile);

}  // namespace bench

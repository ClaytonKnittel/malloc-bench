#include "src/ckmalloc/state_test_fixture.h"

#include "absl/status/status.h"

namespace ckmalloc {

template <>
StateFixture::TestState* StateFixture::TestState::state_ = nullptr;

absl::Status StateFixture::ValidateHeap() {
  return absl::OkStatus();
}

}  // namespace ckmalloc

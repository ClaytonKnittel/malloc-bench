#include "src/ckmalloc/state_test_fixture.h"

#include "absl/status/status.h"
#include "util/absl_util.h"

namespace ckmalloc {

absl::Status StateFixture::ValidateHeap() {
  RETURN_IF_ERROR(slab_manager_test_fixture_->ValidateHeap());
  RETURN_IF_ERROR(metadata_manager_test_fixture_->ValidateHeap());
  RETURN_IF_ERROR(main_allocator_test_fixture_->ValidateHeap());
  return absl::OkStatus();
}

}  // namespace ckmalloc

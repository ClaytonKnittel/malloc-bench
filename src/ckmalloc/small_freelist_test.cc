#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "util/absl_util.h"
#include "util/gtest_util.h"

#include "src/ckmalloc/slab_manager_test_fixture.h"
#include "src/ckmalloc/small_freelist_test_fixture.h"

namespace ckmalloc {

using util::IsOk;

class SmallFreelistTest : public ::testing::Test {
 public:
  SmallFreelistTest()
      : slab_manager_fixture_(std::make_shared<SlabManagerFixture>()),
        small_freelist_fixture_(std::make_shared<SmallFreelistFixture>(
            slab_manager_fixture_->HeapPtr(),
            slab_manager_fixture_->SlabMapPtr(), slab_manager_fixture_,
            slab_manager_fixture_->SlabManagerPtr())) {}

  TestHeap& Heap() {
    return slab_manager_fixture_->Heap();
  }

  SlabManagerFixture::TestSlabManager& SlabManager() {
    return slab_manager_fixture_->SlabManager();
  }

  SmallFreelistFixture::TestSmallFreelist& SmallFreelist() {
    return small_freelist_fixture_->SmallFreelist();
  }

  absl::Status ValidateHeap() {
    RETURN_IF_ERROR(slab_manager_fixture_->ValidateHeap());
    RETURN_IF_ERROR(small_freelist_fixture_->ValidateHeap());
    return absl::OkStatus();
  }

 private:
  std::shared_ptr<SlabManagerFixture> slab_manager_fixture_;
  std::shared_ptr<SmallFreelistFixture> small_freelist_fixture_;
};

TEST_F(SmallFreelistTest, TestEmpty) {
  EXPECT_THAT(ValidateHeap(), IsOk());
}

}  // namespace ckmalloc

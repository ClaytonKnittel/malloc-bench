#include "absl/status/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "util/absl_util.h"
#include "util/gtest_util.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/size_class.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_manager_test_fixture.h"
#include "src/ckmalloc/slice.h"
#include "src/ckmalloc/small_freelist_test_fixture.h"
#include "src/ckmalloc/testlib.h"

namespace ckmalloc {

using testing::UnorderedElementsAreArray;
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

  TestSlabMap& SlabMap() {
    return slab_manager_fixture_->SlabMap();
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

TEST_F(SmallFreelistTest, SingleSlab) {
  AllocatedSlice* slice = SmallFreelist().AllocSlice(16);
  ASSERT_NE(slice, nullptr);

  Slab* slab = SlabMap().FindSlab(SlabManager().PageIdFromPtr(slice));
  ASSERT_NE(slab, nullptr);
  EXPECT_EQ(slab->Type(), SlabType::kSmall);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(Heap().Size(), kPageSize);
}

TEST_F(SmallFreelistTest, Misaligned8) {
  ASSERT_NE(SmallFreelist().AllocSlice(1), nullptr);
  ASSERT_NE(SmallFreelist().AllocSlice(2), nullptr);
  ASSERT_NE(SmallFreelist().AllocSlice(3), nullptr);
  ASSERT_NE(SmallFreelist().AllocSlice(4), nullptr);
  ASSERT_NE(SmallFreelist().AllocSlice(5), nullptr);
  ASSERT_NE(SmallFreelist().AllocSlice(6), nullptr);
  ASSERT_NE(SmallFreelist().AllocSlice(7), nullptr);
  ASSERT_NE(SmallFreelist().AllocSlice(8), nullptr);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(Heap().Size(), kPageSize);
}

TEST_F(SmallFreelistTest, Misaligned16) {
  ASSERT_NE(SmallFreelist().AllocSlice(9), nullptr);
  ASSERT_NE(SmallFreelist().AllocSlice(10), nullptr);
  ASSERT_NE(SmallFreelist().AllocSlice(11), nullptr);
  ASSERT_NE(SmallFreelist().AllocSlice(12), nullptr);
  ASSERT_NE(SmallFreelist().AllocSlice(13), nullptr);
  ASSERT_NE(SmallFreelist().AllocSlice(14), nullptr);
  ASSERT_NE(SmallFreelist().AllocSlice(15), nullptr);
  ASSERT_NE(SmallFreelist().AllocSlice(16), nullptr);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(Heap().Size(), kPageSize);
}

TEST_F(SmallFreelistTest, Misaligned64) {
  ASSERT_NE(SmallFreelist().AllocSlice(49), nullptr);
  ASSERT_NE(SmallFreelist().AllocSlice(55), nullptr);
  ASSERT_NE(SmallFreelist().AllocSlice(63), nullptr);
  ASSERT_NE(SmallFreelist().AllocSlice(64), nullptr);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(Heap().Size(), kPageSize);
}

TEST_F(SmallFreelistTest, FilledSlabs) {
  constexpr size_t kSliceSize = 128;
  for (size_t i = 0; i < kPageSize / kSliceSize; i++) {
    AllocatedSlice* slice = SmallFreelist().AllocSlice(kSliceSize);
    ASSERT_NE(slice, nullptr);
    ASSERT_THAT(ValidateHeap(), IsOk());
  }

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(Heap().Size(), kPageSize);
}

TEST_F(SmallFreelistTest, TwoSlabs) {
  constexpr size_t kSliceSize = 128;
  for (size_t i = 0; i < kPageSize / kSliceSize + 1; i++) {
    AllocatedSlice* slice = SmallFreelist().AllocSlice(kSliceSize);
    ASSERT_NE(slice, nullptr);
    ASSERT_THAT(ValidateHeap(), IsOk());
  }

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(Heap().Size(), 2 * kPageSize);
}

TEST_F(SmallFreelistTest, TwoSizes) {
  EXPECT_NE(SmallFreelist().AllocSlice(32), nullptr);
  EXPECT_NE(SmallFreelist().AllocSlice(64), nullptr);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(Heap().Size(), 2 * kPageSize);
}

TEST_F(SmallFreelistTest, FreeOne) {
  AllocatedSlice* slice = SmallFreelist().AllocSlice(32);
  ASSERT_NE(slice, nullptr);

  SmallFreelist().FreeSlice(slice);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(Heap().Size(), kPageSize);
}

TEST_F(SmallFreelistTest, FreeFullSlab) {
  constexpr size_t kSliceSize = 80;
  std::vector<AllocatedSlice*> slices;
  slices.reserve(kPageSize / kSliceSize);
  for (size_t i = 0; i < kPageSize / kSliceSize; i++) {
    AllocatedSlice* slice = SmallFreelist().AllocSlice(kSliceSize);
    ASSERT_NE(slice, nullptr);
    slices.push_back(slice);
  }

  for (size_t i = 0; i < kPageSize / kSliceSize; i++) {
    size_t idx = (11 * i + 23) % (kPageSize / kSliceSize);
    SmallFreelist().FreeSlice(slices[idx]);
    ASSERT_THAT(ValidateHeap(), IsOk());
  }

  EXPECT_EQ(Heap().Size(), kPageSize);
}

TEST_F(SmallFreelistTest, AllocFreeAllocOne) {
  AllocatedSlice* slice = SmallFreelist().AllocSlice(95);
  ASSERT_NE(slice, nullptr);

  SmallFreelist().FreeSlice(slice);

  EXPECT_EQ(SmallFreelist().AllocSlice(95), slice);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(Heap().Size(), kPageSize);
}

TEST_F(SmallFreelistTest, AllocFreeAllocFull) {
  constexpr size_t kSliceSize = 128;
  std::vector<AllocatedSlice*> slices;
  slices.reserve(kPageSize / kSliceSize);
  for (size_t i = 0; i < kPageSize / kSliceSize; i++) {
    AllocatedSlice* slice = SmallFreelist().AllocSlice(kSliceSize);
    ASSERT_NE(slice, nullptr);
    slices.push_back(slice);
  }

  std::vector<AllocatedSlice*> frees;
  frees.reserve(kPageSize / kSliceSize);
  for (size_t i = 0; i < kPageSize / kSliceSize; i++) {
    size_t idx = (11 * i + 23) % (kPageSize / kSliceSize);
    frees.push_back(slices[idx]);
    SmallFreelist().FreeSlice(slices[idx]);
  }

  std::vector<AllocatedSlice*> slices2;
  slices2.reserve(kPageSize / kSliceSize);
  for (size_t i = 0; i < kPageSize / kSliceSize; i++) {
    AllocatedSlice* slice = SmallFreelist().AllocSlice(kSliceSize);
    ASSERT_NE(slice, nullptr);
    slices2.push_back(slice);
  }

  EXPECT_THAT(slices2, UnorderedElementsAreArray(frees));
  EXPECT_EQ(Heap().Size(), kPageSize);
}

TEST_F(SmallFreelistTest, ManyAllocs) {
  std::vector<AllocatedSlice*> slices;
  for (size_t ord = 0; ord < SizeClass::kNumSizeClasses; ord++) {
    SizeClass size_class = SizeClass::FromOrdinal(ord);
    for (size_t i = 0; i < size_class.MaxSlicesPerSlab(); i++) {
      AllocatedSlice* slice =
          SmallFreelist().AllocSlice(size_class.SliceSize());
      ASSERT_NE(slice, nullptr);
      ASSERT_THAT(ValidateHeap(), IsOk());
      slices.push_back(slice);
    }
  }

  for (size_t i = 0; i < slices.size(); i++) {
    size_t idx = (11 * i + 23) % slices.size();
    SmallFreelist().FreeSlice(slices[idx]);
    ASSERT_THAT(ValidateHeap(), IsOk());
  }

  EXPECT_EQ(Heap().Size(), SizeClass::kNumSizeClasses * kPageSize);
}

}  // namespace ckmalloc

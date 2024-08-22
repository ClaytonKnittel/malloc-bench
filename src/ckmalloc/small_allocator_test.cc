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
#include "src/ckmalloc/small_allocator_test_fixture.h"
#include "src/ckmalloc/testlib.h"

namespace ckmalloc {

using testing::UnorderedElementsAreArray;
using util::IsOk;

class SmallAllocatorTest : public ::testing::Test {
 public:
  static constexpr size_t kNumPages = 64;

  SmallAllocatorTest()
      : heap_(std::make_shared<TestHeap>(kNumPages)),
        slab_map_(std::make_shared<TestSlabMap>()),
        slab_manager_fixture_(
            std::make_shared<SlabManagerFixture>(heap_, slab_map_)),
        small_allocator_fixture_(std::make_shared<SmallAllocatorFixture>(
            heap_, slab_map_, slab_manager_fixture_)) {}

  TestHeap& Heap() {
    return slab_manager_fixture_->Heap();
  }

  TestSlabMap& SlabMap() {
    return slab_manager_fixture_->SlabMap();
  }

  TestSlabManager& SlabManager() {
    return slab_manager_fixture_->SlabManager();
  }

  TestSmallAllocator& SmallAllocator() {
    return small_allocator_fixture_->SmallAllocator();
  }

  void FreeSlice(AllocatedSlice* slice) {
    SmallSlab* slab =
        SlabMap().FindSlab(SlabManager().PageIdFromPtr(slice))->ToSmall();
    SmallAllocator().FreeSlice(slab, slice);
  }

  absl::Status ValidateHeap() {
    RETURN_IF_ERROR(slab_manager_fixture_->ValidateHeap());
    RETURN_IF_ERROR(small_allocator_fixture_->ValidateHeap());
    return absl::OkStatus();
  }

  absl::Status ValidateEmpty() {
    RETURN_IF_ERROR(slab_manager_fixture_->ValidateEmpty());
    RETURN_IF_ERROR(small_allocator_fixture_->ValidateEmpty());
    return absl::OkStatus();
  }

 private:
  std::shared_ptr<TestHeap> heap_;
  std::shared_ptr<TestSlabMap> slab_map_;
  std::shared_ptr<SlabManagerFixture> slab_manager_fixture_;
  std::shared_ptr<SmallAllocatorFixture> small_allocator_fixture_;
};

TEST_F(SmallAllocatorTest, TestEmpty) {
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SmallAllocatorTest, SingleSlab) {
  AllocatedSlice* slice = SmallAllocator().AllocSlice(16);
  ASSERT_NE(slice, nullptr);

  Slab* slab = SlabMap().FindSlab(SlabManager().PageIdFromPtr(slice));
  ASSERT_NE(slab, nullptr);
  EXPECT_EQ(slab->Type(), SlabType::kSmall);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(Heap().Size(), kPageSize);
}

TEST_F(SmallAllocatorTest, Misaligned8) {
  ASSERT_NE(SmallAllocator().AllocSlice(1), nullptr);
  ASSERT_NE(SmallAllocator().AllocSlice(2), nullptr);
  ASSERT_NE(SmallAllocator().AllocSlice(3), nullptr);
  ASSERT_NE(SmallAllocator().AllocSlice(4), nullptr);
  ASSERT_NE(SmallAllocator().AllocSlice(5), nullptr);
  ASSERT_NE(SmallAllocator().AllocSlice(6), nullptr);
  ASSERT_NE(SmallAllocator().AllocSlice(7), nullptr);
  ASSERT_NE(SmallAllocator().AllocSlice(8), nullptr);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(Heap().Size(), kPageSize);
}

TEST_F(SmallAllocatorTest, Misaligned16) {
  ASSERT_NE(SmallAllocator().AllocSlice(9), nullptr);
  ASSERT_NE(SmallAllocator().AllocSlice(10), nullptr);
  ASSERT_NE(SmallAllocator().AllocSlice(11), nullptr);
  ASSERT_NE(SmallAllocator().AllocSlice(12), nullptr);
  ASSERT_NE(SmallAllocator().AllocSlice(13), nullptr);
  ASSERT_NE(SmallAllocator().AllocSlice(14), nullptr);
  ASSERT_NE(SmallAllocator().AllocSlice(15), nullptr);
  ASSERT_NE(SmallAllocator().AllocSlice(16), nullptr);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(Heap().Size(), kPageSize);
}

TEST_F(SmallAllocatorTest, Misaligned64) {
  ASSERT_NE(SmallAllocator().AllocSlice(49), nullptr);
  ASSERT_NE(SmallAllocator().AllocSlice(55), nullptr);
  ASSERT_NE(SmallAllocator().AllocSlice(63), nullptr);
  ASSERT_NE(SmallAllocator().AllocSlice(64), nullptr);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(Heap().Size(), kPageSize);
}

TEST_F(SmallAllocatorTest, FilledSlabs) {
  constexpr size_t kSliceSize = 128;
  for (size_t i = 0; i < kPageSize / kSliceSize; i++) {
    AllocatedSlice* slice = SmallAllocator().AllocSlice(kSliceSize);
    ASSERT_NE(slice, nullptr);
    ASSERT_THAT(ValidateHeap(), IsOk());
  }

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(Heap().Size(), kPageSize);
}

TEST_F(SmallAllocatorTest, TwoSlabs) {
  constexpr size_t kSliceSize = 128;
  for (size_t i = 0; i < kPageSize / kSliceSize + 1; i++) {
    AllocatedSlice* slice = SmallAllocator().AllocSlice(kSliceSize);
    ASSERT_NE(slice, nullptr);
    ASSERT_THAT(ValidateHeap(), IsOk());
  }

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(Heap().Size(), 2 * kPageSize);
}

TEST_F(SmallAllocatorTest, TwoSizes) {
  EXPECT_NE(SmallAllocator().AllocSlice(32), nullptr);
  EXPECT_NE(SmallAllocator().AllocSlice(64), nullptr);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(Heap().Size(), 2 * kPageSize);
}

TEST_F(SmallAllocatorTest, FreeOne) {
  AllocatedSlice* slice = SmallAllocator().AllocSlice(32);
  ASSERT_NE(slice, nullptr);

  FreeSlice(slice);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(ValidateEmpty(), IsOk());
  EXPECT_EQ(Heap().Size(), kPageSize);
}

TEST_F(SmallAllocatorTest, FreeFullSlab) {
  constexpr size_t kSliceSize = 80;
  std::vector<AllocatedSlice*> slices;
  slices.reserve(kPageSize / kSliceSize);
  for (size_t i = 0; i < kPageSize / kSliceSize; i++) {
    AllocatedSlice* slice = SmallAllocator().AllocSlice(kSliceSize);
    ASSERT_NE(slice, nullptr);
    slices.push_back(slice);
  }

  for (size_t i = 0; i < kPageSize / kSliceSize; i++) {
    size_t idx = (11 * i + 23) % (kPageSize / kSliceSize);
    FreeSlice(slices[idx]);
    ASSERT_THAT(ValidateHeap(), IsOk());
  }

  EXPECT_THAT(ValidateEmpty(), IsOk());
  EXPECT_EQ(Heap().Size(), kPageSize);
}

TEST_F(SmallAllocatorTest, AllocFreeAllocOne) {
  AllocatedSlice* slice = SmallAllocator().AllocSlice(95);
  ASSERT_NE(slice, nullptr);

  FreeSlice(slice);

  EXPECT_EQ(SmallAllocator().AllocSlice(95), slice);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(Heap().Size(), kPageSize);
}

TEST_F(SmallAllocatorTest, AllocFreeAllocFull) {
  constexpr size_t kSliceSize = 128;
  std::vector<AllocatedSlice*> slices;
  slices.reserve(kPageSize / kSliceSize);
  for (size_t i = 0; i < kPageSize / kSliceSize; i++) {
    AllocatedSlice* slice = SmallAllocator().AllocSlice(kSliceSize);
    ASSERT_NE(slice, nullptr);
    slices.push_back(slice);
  }

  std::vector<AllocatedSlice*> frees;
  frees.reserve(kPageSize / kSliceSize);
  for (size_t i = 0; i < kPageSize / kSliceSize; i++) {
    size_t idx = (11 * i + 23) % (kPageSize / kSliceSize);
    frees.push_back(slices[idx]);
    FreeSlice(slices[idx]);
  }

  std::vector<AllocatedSlice*> slices2;
  slices2.reserve(kPageSize / kSliceSize);
  for (size_t i = 0; i < kPageSize / kSliceSize; i++) {
    AllocatedSlice* slice = SmallAllocator().AllocSlice(kSliceSize);
    ASSERT_NE(slice, nullptr);
    slices2.push_back(slice);
  }

  EXPECT_THAT(slices2, UnorderedElementsAreArray(frees));
  EXPECT_EQ(Heap().Size(), kPageSize);
}

TEST_F(SmallAllocatorTest, ManyAllocs) {
  std::vector<AllocatedSlice*> slices;
  for (size_t ord = 0; ord < SizeClass::kNumSizeClasses; ord++) {
    SizeClass size_class = SizeClass::FromOrdinal(ord);
    for (size_t i = 0; i < size_class.MaxSlicesPerSlab(); i++) {
      AllocatedSlice* slice =
          SmallAllocator().AllocSlice(size_class.SliceSize());
      ASSERT_NE(slice, nullptr);
      ASSERT_THAT(ValidateHeap(), IsOk());
      slices.push_back(slice);
    }
  }

  for (size_t i = 0; i < slices.size(); i++) {
    size_t idx = (11 * i + 23) % slices.size();
    FreeSlice(slices[idx]);
    ASSERT_THAT(ValidateHeap(), IsOk());
  }

  EXPECT_EQ(Heap().Size(), SizeClass::kNumSizeClasses * kPageSize);
}

}  // namespace ckmalloc

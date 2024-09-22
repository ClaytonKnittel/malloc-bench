#include "absl/status/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "util/absl_util.h"
#include "util/gtest_util.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/freelist.h"
#include "src/ckmalloc/size_class.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_manager_test_fixture.h"
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
        freelist_(std::make_shared<Freelist>()),
        small_allocator_fixture_(std::make_shared<SmallAllocatorFixture>(
            heap_, slab_map_, slab_manager_fixture_, freelist_)) {}

  TestHeap& Heap() {
    return *heap_;
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

  void FreeSmall(Void* ptr) {
    SmallSlab* slab =
        SlabMap().FindSlab(SlabManager().PageIdFromPtr(ptr))->ToSmall();
    SmallAllocator().FreeSmall(slab, ptr);
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
  std::shared_ptr<Freelist> freelist_;
  std::shared_ptr<SmallAllocatorFixture> small_allocator_fixture_;
};

TEST_F(SmallAllocatorTest, TestEmpty) {
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SmallAllocatorTest, SingleSlab) {
  void* ptr = SmallAllocator().AllocSmall(16);
  ASSERT_NE(ptr, nullptr);

  Slab* slab = SlabMap().FindSlab(SlabManager().PageIdFromPtr(ptr));
  ASSERT_NE(slab, nullptr);
  EXPECT_EQ(slab->Type(), SlabType::kSmall);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(Heap().Size(), kPageSize);
}

TEST_F(SmallAllocatorTest, Misaligned8) {
  ASSERT_NE(SmallAllocator().AllocSmall(1), nullptr);
  ASSERT_NE(SmallAllocator().AllocSmall(2), nullptr);
  ASSERT_NE(SmallAllocator().AllocSmall(3), nullptr);
  ASSERT_NE(SmallAllocator().AllocSmall(4), nullptr);
  ASSERT_NE(SmallAllocator().AllocSmall(5), nullptr);
  ASSERT_NE(SmallAllocator().AllocSmall(6), nullptr);
  ASSERT_NE(SmallAllocator().AllocSmall(7), nullptr);
  ASSERT_NE(SmallAllocator().AllocSmall(8), nullptr);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(Heap().Size(), kPageSize);
}

TEST_F(SmallAllocatorTest, Misaligned16) {
  ASSERT_NE(SmallAllocator().AllocSmall(9), nullptr);
  ASSERT_NE(SmallAllocator().AllocSmall(10), nullptr);
  ASSERT_NE(SmallAllocator().AllocSmall(11), nullptr);
  ASSERT_NE(SmallAllocator().AllocSmall(12), nullptr);
  ASSERT_NE(SmallAllocator().AllocSmall(13), nullptr);
  ASSERT_NE(SmallAllocator().AllocSmall(14), nullptr);
  ASSERT_NE(SmallAllocator().AllocSmall(15), nullptr);
  ASSERT_NE(SmallAllocator().AllocSmall(16), nullptr);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(Heap().Size(), kPageSize);
}

TEST_F(SmallAllocatorTest, Misaligned64) {
  ASSERT_NE(SmallAllocator().AllocSmall(49), nullptr);
  ASSERT_NE(SmallAllocator().AllocSmall(55), nullptr);
  ASSERT_NE(SmallAllocator().AllocSmall(63), nullptr);
  ASSERT_NE(SmallAllocator().AllocSmall(64), nullptr);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(Heap().Size(), kPageSize);
}

TEST_F(SmallAllocatorTest, FilledSlabs) {
  constexpr size_t kSliceSize = 128;
  for (size_t i = 0; i < kPageSize / kSliceSize; i++) {
    void* ptr = SmallAllocator().AllocSmall(kSliceSize);
    ASSERT_NE(ptr, nullptr);
    ASSERT_THAT(ValidateHeap(), IsOk());
  }

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(Heap().Size(), kPageSize);
}

TEST_F(SmallAllocatorTest, TwoSlabs) {
  constexpr size_t kSliceSize = 128;
  for (size_t i = 0; i < kPageSize / kSliceSize + 1; i++) {
    void* ptr = SmallAllocator().AllocSmall(kSliceSize);
    ASSERT_NE(ptr, nullptr);
    ASSERT_THAT(ValidateHeap(), IsOk());
  }

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(Heap().Size(), 2 * kPageSize);
}

TEST_F(SmallAllocatorTest, TwoSizes) {
  EXPECT_NE(SmallAllocator().AllocSmall(32), nullptr);
  EXPECT_NE(SmallAllocator().AllocSmall(64), nullptr);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(Heap().Size(), 2 * kPageSize);
}

TEST_F(SmallAllocatorTest, FreeOne) {
  Void* ptr = SmallAllocator().AllocSmall(32);
  ASSERT_NE(ptr, nullptr);

  FreeSmall(ptr);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(ValidateEmpty(), IsOk());
  EXPECT_EQ(Heap().Size(), kPageSize);
}

TEST_F(SmallAllocatorTest, FreeFullSlab) {
  constexpr size_t kSliceSize = 80;
  std::vector<Void*> ptrs;
  ptrs.reserve(kPageSize / kSliceSize);
  for (size_t i = 0; i < kPageSize / kSliceSize; i++) {
    Void* ptr = SmallAllocator().AllocSmall(kSliceSize);
    ASSERT_NE(ptr, nullptr);
    ptrs.push_back(ptr);
  }

  for (size_t i = 0; i < kPageSize / kSliceSize; i++) {
    size_t idx = (11 * i + 23) % (kPageSize / kSliceSize);
    FreeSmall(ptrs[idx]);
    ASSERT_THAT(ValidateHeap(), IsOk());
  }

  EXPECT_THAT(ValidateEmpty(), IsOk());
  EXPECT_EQ(Heap().Size(), kPageSize);
}

TEST_F(SmallAllocatorTest, AllocFreeAllocOne) {
  Void* ptr = SmallAllocator().AllocSmall(95);
  ASSERT_NE(ptr, nullptr);

  FreeSmall(ptr);

  EXPECT_EQ(SmallAllocator().AllocSmall(95), ptr);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(Heap().Size(), kPageSize);
}

TEST_F(SmallAllocatorTest, AllocFreeAllocFull) {
  constexpr size_t kSliceSize = 128;
  std::vector<Void*> ptrs;
  ptrs.reserve(kPageSize / kSliceSize);
  for (size_t i = 0; i < kPageSize / kSliceSize; i++) {
    Void* ptr = SmallAllocator().AllocSmall(kSliceSize);
    ASSERT_NE(ptr, nullptr);
    ptrs.push_back(ptr);
  }

  std::vector<Void*> frees;
  frees.reserve(kPageSize / kSliceSize);
  for (size_t i = 0; i < kPageSize / kSliceSize; i++) {
    size_t idx = (11 * i + 23) % (kPageSize / kSliceSize);
    frees.push_back(ptrs[idx]);
    FreeSmall(ptrs[idx]);
  }

  std::vector<void*> ptrs2;
  ptrs2.reserve(kPageSize / kSliceSize);
  for (size_t i = 0; i < kPageSize / kSliceSize; i++) {
    void* ptr = SmallAllocator().AllocSmall(kSliceSize);
    ASSERT_NE(ptr, nullptr);
    ptrs2.push_back(ptr);
  }

  EXPECT_THAT(ptrs2, UnorderedElementsAreArray(frees));
  EXPECT_EQ(Heap().Size(), kPageSize);
}

TEST_F(SmallAllocatorTest, ManyAllocs) {
  std::vector<Void*> ptrs;
  for (size_t ord = 0; ord < SizeClass::kNumSizeClasses; ord++) {
    SizeClass size_class = SizeClass::FromOrdinal(ord);
    for (size_t i = 0; i < size_class.MaxSlicesPerSlab(); i++) {
      Void* ptr = SmallAllocator().AllocSmall(size_class.SliceSize());
      ASSERT_NE(ptr, nullptr);
      ASSERT_THAT(ValidateHeap(), IsOk());
      ptrs.push_back(ptr);
    }
  }

  for (size_t i = 0; i < ptrs.size(); i++) {
    size_t idx = (11 * i + 23) % ptrs.size();
    FreeSmall(ptrs[idx]);
    ASSERT_THAT(ValidateHeap(), IsOk());
  }

  EXPECT_EQ(Heap().Size(), SizeClass::kNumSizeClasses * kPageSize);
}

}  // namespace ckmalloc

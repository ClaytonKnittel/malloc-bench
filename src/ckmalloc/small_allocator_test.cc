#include <ranges>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
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
#include "src/ckmalloc/util.h"

namespace ckmalloc {

using testing::UnorderedElementsAreArray;
using util::IsOk;

class SmallAllocatorTest : public ::testing::Test {
 public:
  static constexpr size_t kHeapSize = 64 * kPageSize;

  SmallAllocatorTest()
      : heap_factory_(std::make_shared<TestHeapFactory>()),
        slab_map_(std::make_shared<TestSlabMap>()),
        slab_manager_fixture_(std::make_shared<SlabManagerFixture>(
            heap_factory_, slab_map_, kHeapSize)),
        freelist_(std::make_shared<Freelist>()),
        small_allocator_fixture_(std::make_shared<SmallAllocatorFixture>(
            slab_map_, slab_manager_fixture_, freelist_)) {
    TestSysAlloc::NewInstance(heap_factory_.get());
  }

  ~SmallAllocatorTest() override {
    TestSysAlloc::Reset();
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

  static size_t TotalHeapsSize() {
    return SlabManagerFixture::TotalHeapsSize();
  }

  void FreeSmall(Void* ptr) {
    SmallSlab* slab = SlabMap().FindSlab(PageId::FromPtr(ptr))->ToSmall();
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
  std::shared_ptr<TestHeapFactory> heap_factory_;
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

  Slab* slab = SlabMap().FindSlab(PageId::FromPtr(ptr));
  ASSERT_NE(slab, nullptr);
  EXPECT_EQ(slab->Type(), SlabType::kSmall);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(TotalHeapsSize(), kPageSize);
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
  EXPECT_EQ(TotalHeapsSize(), kPageSize);
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
  EXPECT_EQ(TotalHeapsSize(), kPageSize);
}

TEST_F(SmallAllocatorTest, Misaligned64) {
  ASSERT_NE(SmallAllocator().AllocSmall(49), nullptr);
  ASSERT_NE(SmallAllocator().AllocSmall(55), nullptr);
  ASSERT_NE(SmallAllocator().AllocSmall(63), nullptr);
  ASSERT_NE(SmallAllocator().AllocSmall(64), nullptr);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(TotalHeapsSize(), kPageSize);
}

TEST_F(SmallAllocatorTest, TwoSizes) {
  EXPECT_NE(SmallAllocator().AllocSmall(32), nullptr);
  EXPECT_NE(SmallAllocator().AllocSmall(64), nullptr);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(TotalHeapsSize(), 2 * kPageSize);
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

  EXPECT_THAT(ValidateEmpty(), IsOk());
}

class SizeClassTest : public SmallAllocatorTest,
                      public ::testing::WithParamInterface<SizeClass> {
 public:
  static SizeClass SizeClass() {
    return GetParam();
  }
};

TEST_P(SizeClassTest, FilledSlabs) {
  for (size_t i = 0; i < SizeClass().MaxSlicesPerSlab(); i++) {
    void* ptr = SmallAllocator().AllocSmall(SizeClass().SliceSize());
    ASSERT_NE(ptr, nullptr);
    ASSERT_THAT(ValidateHeap(), IsOk());
  }

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(TotalHeapsSize(), SizeClass().Pages() * kPageSize);
}

TEST_P(SizeClassTest, TwoSlabs) {
  for (size_t i = 0; i < SizeClass().MaxSlicesPerSlab() + 1; i++) {
    void* ptr = SmallAllocator().AllocSmall(SizeClass().SliceSize());
    ASSERT_NE(ptr, nullptr);
    ASSERT_THAT(ValidateHeap(), IsOk());
  }

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(TotalHeapsSize(), 2 * SizeClass().Pages() * kPageSize);
}

TEST_P(SizeClassTest, FreeOne) {
  Void* ptr = SmallAllocator().AllocSmall(SizeClass().SliceSize());
  ASSERT_NE(ptr, nullptr);

  FreeSmall(ptr);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(ValidateEmpty(), IsOk());
}

TEST_P(SizeClassTest, FreeFullSlab) {
  std::vector<Void*> ptrs;
  ptrs.reserve(SizeClass().MaxSlicesPerSlab());
  for (size_t i = 0; i < SizeClass().MaxSlicesPerSlab(); i++) {
    Void* ptr = SmallAllocator().AllocSmall(SizeClass().SliceSize());
    ASSERT_NE(ptr, nullptr);
    ptrs.push_back(ptr);
  }

  for (size_t i = 0; i < SizeClass().MaxSlicesPerSlab(); i++) {
    size_t idx = (11 * i + 23) % (SizeClass().MaxSlicesPerSlab());
    FreeSmall(ptrs[idx]);
    ASSERT_THAT(ValidateHeap(), IsOk());
  }

  EXPECT_THAT(ValidateEmpty(), IsOk());
}

TEST_P(SizeClassTest, AllocFreeAllocOne) {
  Void* ptr = SmallAllocator().AllocSmall(SizeClass().SliceSize());
  ASSERT_NE(ptr, nullptr);

  FreeSmall(ptr);

  EXPECT_EQ(SmallAllocator().AllocSmall(SizeClass().SliceSize()), ptr);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(TotalHeapsSize(), SizeClass().Pages() * kPageSize);
}

TEST_P(SizeClassTest, AllocFreeAllocFull) {
  std::vector<Void*> ptrs;
  ptrs.reserve(SizeClass().MaxSlicesPerSlab());
  for (size_t i = 0; i < SizeClass().MaxSlicesPerSlab(); i++) {
    Void* ptr = SmallAllocator().AllocSmall(SizeClass().SliceSize());
    ASSERT_NE(ptr, nullptr);
    ptrs.push_back(ptr);
  }

  std::vector<Void*> frees;
  frees.reserve(SizeClass().MaxSlicesPerSlab() - 1);
  // Don't free the whole slab to prevent it from being reallocated (potentially
  // to a different location).
  for (size_t i = 0; i < SizeClass().MaxSlicesPerSlab() - 1; i++) {
    size_t idx = (11 * i + 23) % (SizeClass().MaxSlicesPerSlab());
    frees.push_back(ptrs[idx]);
    FreeSmall(ptrs[idx]);
  }

  std::vector<void*> ptrs2;
  ptrs2.reserve(SizeClass().MaxSlicesPerSlab() - 1);
  for (size_t i = 0; i < SizeClass().MaxSlicesPerSlab() - 1; i++) {
    void* ptr = SmallAllocator().AllocSmall(SizeClass().SliceSize());
    ASSERT_NE(ptr, nullptr);
    ptrs2.push_back(ptr);
  }

  EXPECT_THAT(ptrs2, UnorderedElementsAreArray(frees));
  EXPECT_EQ(TotalHeapsSize(), SizeClass().Pages() * kPageSize);
}

static const auto kAllSizeClasses =
    testing::ValuesIn(std::ranges::iota_view{
                          static_cast<uint32_t>(0),
                          static_cast<uint32_t>(SizeClass::kNumSizeClasses) } |
                      std::views::transform(SizeClass::FromOrdinal) |
                      RangeToContainer<std::vector<SizeClass>>());

INSTANTIATE_TEST_SUITE_P(SizeClassSuite, SizeClassTest, kAllSizeClasses,
                         [](const testing::TestParamInfo<SizeClass>& info) {
                           return absl::StrCat(info.param.SliceSize());
                         });

}  // namespace ckmalloc

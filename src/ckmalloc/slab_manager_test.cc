#include <cstddef>
#include <memory>

#include "gtest/gtest.h"
#include "util/gtest_util.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_manager_test_fixture.h"
#include "src/ckmalloc/testlib.h"

namespace ckmalloc {

using util::IsOk;

class SlabManagerTest : public testing::Test {
 public:
  static constexpr size_t kNumPages = 64;

  SlabManagerTest()
      : heap_factory_(std::make_shared<TestHeapFactory>(kNumPages * kPageSize)),
        slab_map_(std::make_shared<TestSlabMap>()),
        test_fixture_(heap_factory_, slab_map_) {}

  TestHeapFactory& HeapFactory() {
    return *heap_factory_;
  }

  TestSlabManager& SlabManager() {
    return test_fixture_.SlabManager();
  }

  SlabManagerFixture& Fixture() {
    return test_fixture_;
  }

  absl::Status ValidateHeap() {
    return test_fixture_.ValidateHeap();
  }

  absl::Status ValidateEmpty() {
    return test_fixture_.ValidateEmpty();
  }

 private:
  std::shared_ptr<TestHeapFactory> heap_factory_;
  std::shared_ptr<TestSlabMap> slab_map_;
  SlabManagerFixture test_fixture_;
};

TEST_F(SlabManagerTest, HeapStartIsPageIdZero) {
  ASSERT_THAT(Fixture().AllocateSlab(1), IsOk());
  ASSERT_EQ(HeapFactory().Instances().size(), 1);
  EXPECT_EQ(SlabManager().PageIdFromPtr(HeapFactory().Instance(0)->Start()),
            PageId(0));
}

TEST_F(SlabManagerTest, AllPtrsInFirstPageIdZero) {
  ASSERT_THAT(Fixture().AllocateSlab(1), IsOk());
  for (size_t offset = 0; offset < kPageSize; offset++) {
    EXPECT_EQ(
        SlabManager().PageIdFromPtr(
            static_cast<uint8_t*>(HeapFactory().Instance(0)->Start()) + offset),
        PageId(0));
  }
}

TEST_F(SlabManagerTest, PageIdIncreasesPerPage) {
  constexpr size_t kPages = 16;
  ASSERT_THAT(Fixture().AllocateSlab(kPages), IsOk());
  for (size_t page_n = 0; page_n < kPages; page_n++) {
    void* beginning =
        static_cast<uint8_t*>(HeapFactory().Instance(0)->Start()) +
        page_n * kPageSize;
    void* end = static_cast<uint8_t*>(beginning) + kPageSize - 1;
    EXPECT_EQ(SlabManager().PageIdFromPtr(beginning), PageId(page_n));
    EXPECT_EQ(SlabManager().PageIdFromPtr(end), PageId(page_n));
  }
}

TEST_F(SlabManagerTest, SlabStartFromId) {
  constexpr size_t kPages = 16;
  ASSERT_THAT(Fixture().AllocateSlab(kPages), IsOk());
  for (size_t page_n = 0; page_n < kPages; page_n++) {
    EXPECT_EQ(SlabManager().PageStartFromId(PageId(page_n)),
              static_cast<uint8_t*>(HeapFactory().Instance(0)->Start()) +
                  page_n * kPageSize);
  }
}

TEST_F(SlabManagerTest, EmptyHeapValid) {
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(SlabManagerTest, SinglePageHeapValid) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab, Fixture().AllocateSlab(1));
  EXPECT_EQ(slab->StartId(), PageId::Zero());
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, TwoAdjacentAllocatedSlabs) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab1, Fixture().AllocateSlab(1));
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab2, Fixture().AllocateSlab(1));
  EXPECT_EQ(slab1->StartId(), PageId::Zero());
  EXPECT_EQ(slab2->StartId(), PageId(1));
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, SingleLargeSlab) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab, Fixture().AllocateSlab(9));
  EXPECT_EQ(slab->StartId(), PageId::Zero());
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, SlabTooLargeDoesNotAllocate) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab,
                       Fixture().AllocateSlab(kNumPages + 1));
  EXPECT_EQ(slab, nullptr);
  EXPECT_EQ(HeapFactory().Instance(0)->Size(), 0);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, FreeOnce) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab, Fixture().AllocateSlab(1));
  ASSERT_THAT(Fixture().FreeSlab(slab), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(SlabManagerTest, FreeLarge) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab, Fixture().AllocateSlab(12));
  ASSERT_THAT(Fixture().FreeSlab(slab), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(SlabManagerTest, FreeTwice) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab1, Fixture().AllocateSlab(1));
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab2, Fixture().AllocateSlab(1));
  ASSERT_THAT(Fixture().FreeSlab(slab1), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  ASSERT_THAT(Fixture().FreeSlab(slab2), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(SlabManagerTest, CoalesceBehind) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab1, Fixture().AllocateSlab(3));
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab2, Fixture().AllocateSlab(5));
  ASSERT_THAT(Fixture().FreeSlab(slab2), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  ASSERT_THAT(Fixture().FreeSlab(slab1), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(SlabManagerTest, CoalesceAhead) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab1, Fixture().AllocateSlab(2));
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab2, Fixture().AllocateSlab(5));
  ASSERT_THAT(Fixture().FreeSlab(slab1), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  ASSERT_THAT(Fixture().FreeSlab(slab2), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(SlabManagerTest, CoalesceBothDirections) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab1, Fixture().AllocateSlab(2));
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab2, Fixture().AllocateSlab(1));
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab3, Fixture().AllocateSlab(3));
  ASSERT_THAT(Fixture().FreeSlab(slab1), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  ASSERT_THAT(Fixture().FreeSlab(slab3), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  ASSERT_THAT(Fixture().FreeSlab(slab2), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(SlabManagerTest, ReAllocateFreed) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab1, Fixture().AllocateSlab(2));
  ASSERT_THAT(Fixture().FreeSlab(slab1), IsOk());
  ASSERT_THAT(Fixture().AllocateSlab(1).status(), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(HeapFactory().Instance(0)->Size(), 2 * kPageSize);
}

TEST_F(SlabManagerTest, ExtendHeapWithFreeAtEnd) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab1, Fixture().AllocateSlab(2));
  ASSERT_THAT(Fixture().FreeSlab(slab1), IsOk());
  ASSERT_THAT(Fixture().AllocateSlab(3).status(), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(HeapFactory().Instance(0)->Size(), 3 * kPageSize);
}

TEST_F(SlabManagerTest, BestFit) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab1, Fixture().AllocateSlab(3));
  ASSERT_THAT(Fixture().AllocateSlab(1).status(), IsOk());
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab3, Fixture().AllocateSlab(6));
  ASSERT_THAT(Fixture().AllocateSlab(1).status(), IsOk());
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab5, Fixture().AllocateSlab(4));
  ASSERT_THAT(Fixture().AllocateSlab(1).status(), IsOk());
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab7, Fixture().AllocateSlab(8));
  PageId slab5_start = slab5->StartId();

  // Free all the larger slabs, which should have alternatively fill the heap.
  ASSERT_THAT(Fixture().FreeSlab(slab1), IsOk());
  ASSERT_THAT(Fixture().FreeSlab(slab3), IsOk());
  ASSERT_THAT(Fixture().FreeSlab(slab5), IsOk());
  ASSERT_THAT(Fixture().FreeSlab(slab7), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  // There should now be free slabs of size 3, 4, 6, and 8.

  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab8, Fixture().AllocateSlab(4));

  // We should have found the perfect fit, which used to be slab 5.
  EXPECT_EQ(slab8->StartId(), slab5_start);
  EXPECT_EQ(HeapFactory().Instance(0)->Size(), 24 * kPageSize);
}

}  // namespace ckmalloc

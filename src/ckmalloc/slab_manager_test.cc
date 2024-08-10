#include <cstddef>

#include "gtest/gtest.h"
#include "util/gtest_util.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_manager_test_fixture.h"

namespace ckmalloc {

using util::IsOk;

class SlabManagerTest : public testing::Test, public SlabManagerFixture {};

TEST_F(SlabManagerTest, HeapStartIsPageIdZero) {
  ASSERT_THAT(AllocateSlab(1), IsOk());
  EXPECT_EQ(SlabManager().PageIdFromPtr(Heap().Start()), PageId(0));
}

TEST_F(SlabManagerTest, AllPtrsInFirstPageIdZero) {
  ASSERT_THAT(AllocateSlab(1), IsOk());
  for (size_t offset = 0; offset < kPageSize; offset++) {
    EXPECT_EQ(SlabManager().PageIdFromPtr(
                  static_cast<uint8_t*>(Heap().Start()) + offset),
              PageId(0));
  }
}

TEST_F(SlabManagerTest, PageIdIncreasesPerPage) {
  constexpr size_t kPages = 16;
  ASSERT_THAT(AllocateSlab(kPages), IsOk());
  for (size_t page_n = 0; page_n < kPages; page_n++) {
    void* beginning =
        static_cast<uint8_t*>(Heap().Start()) + page_n * kPageSize;
    void* end = static_cast<uint8_t*>(beginning) + kPageSize - 1;
    EXPECT_EQ(SlabManager().PageIdFromPtr(beginning), PageId(page_n));
    EXPECT_EQ(SlabManager().PageIdFromPtr(end), PageId(page_n));
  }
}

TEST_F(SlabManagerTest, SlabStartFromId) {
  constexpr size_t kPages = 16;
  ASSERT_THAT(AllocateSlab(kPages), IsOk());
  for (size_t page_n = 0; page_n < kPages; page_n++) {
    EXPECT_EQ(SlabManager().PageStartFromId(PageId(page_n)),
              static_cast<uint8_t*>(Heap().Start()) + page_n * kPageSize);
  }
}

TEST_F(SlabManagerTest, EmptyHeapValid) {
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, SinglePageHeapValid) {
  ASSERT_OK_AND_DEFINE(Slab*, slab, AllocateSlab(1));
  EXPECT_EQ(slab->StartId(), PageId::Zero());
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, TwoAdjacentAllocatedSlabs) {
  ASSERT_OK_AND_DEFINE(Slab*, slab1, AllocateSlab(1));
  ASSERT_OK_AND_DEFINE(Slab*, slab2, AllocateSlab(1));
  EXPECT_EQ(slab1->StartId(), PageId::Zero());
  EXPECT_EQ(slab2->StartId(), PageId(1));
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, SingleLargeSlab) {
  ASSERT_OK_AND_DEFINE(Slab*, slab, AllocateSlab(9));
  EXPECT_EQ(slab->StartId(), PageId::Zero());
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, SlabTooLargeDoesNotAllocate) {
  ASSERT_OK_AND_DEFINE(Slab*, slab, AllocateSlab(kNumPages + 1));
  EXPECT_EQ(slab, nullptr);
  EXPECT_EQ(Heap().Size(), 0);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, FreeOnce) {
  ASSERT_OK_AND_DEFINE(Slab*, slab, AllocateSlab(1));
  ASSERT_THAT(FreeSlab(slab), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, FreeLarge) {
  ASSERT_OK_AND_DEFINE(Slab*, slab, AllocateSlab(12));
  ASSERT_THAT(FreeSlab(slab), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, FreeTwice) {
  ASSERT_OK_AND_DEFINE(Slab*, slab1, AllocateSlab(1));
  ASSERT_OK_AND_DEFINE(Slab*, slab2, AllocateSlab(1));
  ASSERT_THAT(FreeSlab(slab1), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  ASSERT_THAT(FreeSlab(slab2), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, CoalesceBehind) {
  ASSERT_OK_AND_DEFINE(Slab*, slab1, AllocateSlab(3));
  ASSERT_OK_AND_DEFINE(Slab*, slab2, AllocateSlab(5));
  ASSERT_THAT(FreeSlab(slab2), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  ASSERT_THAT(FreeSlab(slab1), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, CoalesceAhead) {
  ASSERT_OK_AND_DEFINE(Slab*, slab1, AllocateSlab(2));
  ASSERT_OK_AND_DEFINE(Slab*, slab2, AllocateSlab(5));
  ASSERT_THAT(FreeSlab(slab1), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  ASSERT_THAT(FreeSlab(slab2), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, CoalesceBothDirections) {
  ASSERT_OK_AND_DEFINE(Slab*, slab1, AllocateSlab(2));
  ASSERT_OK_AND_DEFINE(Slab*, slab2, AllocateSlab(1));
  ASSERT_OK_AND_DEFINE(Slab*, slab3, AllocateSlab(3));
  ASSERT_THAT(FreeSlab(slab1), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  ASSERT_THAT(FreeSlab(slab3), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  ASSERT_THAT(FreeSlab(slab2), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, ReAllocateFreed) {
  ASSERT_OK_AND_DEFINE(Slab*, slab1, AllocateSlab(2));
  ASSERT_THAT(FreeSlab(slab1), IsOk());
  ASSERT_THAT(AllocateSlab(1).status(), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(Heap().Size(), 2 * kPageSize);
}

TEST_F(SlabManagerTest, ExtendHeapWithFreeAtEnd) {
  ASSERT_OK_AND_DEFINE(Slab*, slab1, AllocateSlab(2));
  ASSERT_THAT(FreeSlab(slab1), IsOk());
  ASSERT_THAT(AllocateSlab(3).status(), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(Heap().Size(), 3 * kPageSize);
}

TEST_F(SlabManagerTest, BestFit) {
  ASSERT_OK_AND_DEFINE(Slab*, slab1, AllocateSlab(3));
  ASSERT_THAT(AllocateSlab(1).status(), IsOk());
  ASSERT_OK_AND_DEFINE(Slab*, slab3, AllocateSlab(6));
  ASSERT_THAT(AllocateSlab(1).status(), IsOk());
  ASSERT_OK_AND_DEFINE(Slab*, slab5, AllocateSlab(4));
  ASSERT_THAT(AllocateSlab(1).status(), IsOk());
  ASSERT_OK_AND_DEFINE(Slab*, slab7, AllocateSlab(8));
  PageId slab5_start = slab5->StartId();

  // Free all the larger slabs, which should have alternatively fill the heap.
  ASSERT_THAT(FreeSlab(slab1), IsOk());
  ASSERT_THAT(FreeSlab(slab3), IsOk());
  ASSERT_THAT(FreeSlab(slab5), IsOk());
  ASSERT_THAT(FreeSlab(slab7), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  // There should now be free slabs of size 3, 4, 6, and 8.

  ASSERT_OK_AND_DEFINE(Slab*, slab8, AllocateSlab(4));

  // We should have found the perfect fit, which used to be slab 5.
  EXPECT_EQ(slab8->StartId(), slab5_start);
  EXPECT_EQ(Heap().Size(), 24 * kPageSize);
}

}  // namespace ckmalloc

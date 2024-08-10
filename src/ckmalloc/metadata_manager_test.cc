#include <algorithm>
#include <cstdint>
#include <random>

#include "gmock/gmock.h"
#include "util/gtest_util.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/metadata_manager_test_fixture.h"
#include "src/ckmalloc/slab.h"

namespace ckmalloc {

using testing::UnorderedElementsAreArray;
using util::IsOk;
using util::IsOkAndHolds;

TEST_F(MetadataManagerTest, TestEmpty) {
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(SlabMetaFreelistLength(), IsOkAndHolds(0));
}

TEST_F(MetadataManagerTest, AllocateOnce) {
  ASSERT_OK_AND_DEFINE(void*, value, Alloc(16));
  EXPECT_EQ(value, Heap().Start());
}

TEST_F(MetadataManagerTest, AllocateAdjacent) {
  ASSERT_OK_AND_DEFINE(void*, v1, Alloc(7));
  ASSERT_OK_AND_DEFINE(void*, v2, Alloc(41));
  ASSERT_OK_AND_DEFINE(void*, v3, Alloc(60));
  EXPECT_EQ(reinterpret_cast<uint8_t*>(v2) - reinterpret_cast<uint8_t*>(v1), 7);
  EXPECT_EQ(reinterpret_cast<uint8_t*>(v3) - reinterpret_cast<uint8_t*>(v2),
            41);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MetadataManagerTest, AllocateAligned) {
  ASSERT_OK_AND_DEFINE(void*, v1, Alloc(7));
  // Should range from 8 - 55 (inclusive)
  ASSERT_OK_AND_DEFINE(void*, v2, Alloc(48, 8));
  // Should range from 64 - 127 (inclusive)
  ASSERT_OK_AND_DEFINE(void*, v3, Alloc(64, 64));
  EXPECT_EQ(reinterpret_cast<uint8_t*>(v2) - reinterpret_cast<uint8_t*>(v1), 8);
  EXPECT_EQ(reinterpret_cast<uint8_t*>(v3) - reinterpret_cast<uint8_t*>(v2),
            56);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MetadataManagerTest, AllocateLarge) {
  ASSERT_OK_AND_DEFINE(void*, value, Alloc(kPageSize));
  EXPECT_EQ(value, Heap().Start());
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MetadataManagerTest, AllocateExtraLarge) {
  ASSERT_OK_AND_DEFINE(void*, value, Alloc(11 * kPageSize));
  EXPECT_EQ(value, Heap().Start());
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MetadataManagerTest, AllocateAndStay) {
  ASSERT_THAT(Alloc(kPageSize / 2).status(), IsOk());
  ASSERT_OK_AND_DEFINE(void*, v2, Alloc(3 * kPageSize / 4));
  // v2 should be allocated in a new page by itself.
  EXPECT_EQ(v2, SlabManager().PageStartFromId(PageId(1)));
  EXPECT_THAT(ValidateHeap(), IsOk());

  // Since the remainder in the first slab was higher, it should continue to be
  // allocated from.
  ASSERT_OK_AND_DEFINE(void*, v3, Alloc(kPageSize / 2));
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(v3, static_cast<uint8_t*>(Heap().Start()) + kPageSize / 2);
  EXPECT_EQ(Heap().Size(), 2 * kPageSize);
}

TEST_F(MetadataManagerTest, AllocateAndSwitch) {
  ASSERT_THAT(Alloc(3 * kPageSize / 4).status(), IsOk());
  ASSERT_OK_AND_DEFINE(void*, v2, Alloc(kPageSize / 2));
  // v2 should be allocated in a new page by itself.
  EXPECT_EQ(v2, SlabManager().PageStartFromId(PageId(1)));
  EXPECT_THAT(ValidateHeap(), IsOk());

  // Since the remainder in the second slab was higher, it should continue to be
  // allocated from.
  ASSERT_OK_AND_DEFINE(void*, v3, Alloc(kPageSize / 2));
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(v3, static_cast<uint8_t*>(Heap().Start()) + 3 * kPageSize / 2);
  EXPECT_EQ(Heap().Size(), 2 * kPageSize);
}

TEST_F(MetadataManagerTest, AllocateLargeAndStay) {
  ASSERT_THAT(Alloc(32).status(), IsOk());
  ASSERT_OK_AND_DEFINE(void*, v2, Alloc(kPageSize + 64));
  // v2 should be allocated in a new slab by itself since it is so large.
  EXPECT_EQ(v2, SlabManager().PageStartFromId(PageId(1)));
  EXPECT_THAT(ValidateHeap(), IsOk());

  // Since the remainder in the first slab was higher, it should continue to be
  // allocated from.
  ASSERT_OK_AND_DEFINE(void*, v3, Alloc(kPageSize - 32));
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(v3, static_cast<uint8_t*>(Heap().Start()) + 32);
  EXPECT_EQ(Heap().Size(), 3 * kPageSize);
}

TEST_F(MetadataManagerTest, AllocateLargeAndSwitch) {
  ASSERT_THAT(Alloc(64).status(), IsOk());
  ASSERT_OK_AND_DEFINE(void*, v2, Alloc(kPageSize + 32));
  // v2 should be allocated in a new slab by itself since it is so large.
  EXPECT_EQ(v2, SlabManager().PageStartFromId(PageId(1)));
  EXPECT_THAT(ValidateHeap(), IsOk());

  // Since the remainder in the second slab was higher, it should continue to be
  // allocated from.
  ASSERT_OK_AND_DEFINE(void*, v3, Alloc(kPageSize - 32));
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(v3, static_cast<uint8_t*>(Heap().Start()) + 2 * kPageSize + 32);
  EXPECT_EQ(Heap().Size(), 3 * kPageSize);
}

TEST_F(MetadataManagerTest, AllocateWithOtherAllocators) {
  ASSERT_THAT(Alloc(kPageSize).status(), IsOk());

  // Allocate a phony slab right after the one just allocated.
  auto res = SlabManager().Alloc(1, SlabType::kSmall);
  ASSERT_TRUE(res.has_value());
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(res.value().first, PageId(1));

  // Allocate another slab-sized metadata alloc.
  ASSERT_OK_AND_DEFINE(void*, v2, Alloc(kPageSize));
  // v2 should be allocated in a new slab after the two already-allocated slabs.
  EXPECT_EQ(v2, SlabManager().PageStartFromId(PageId(2)));
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(Heap().Size(), 3 * kPageSize);
}

TEST_F(MetadataManagerTest, AllocateSlabMeta) {
  ASSERT_OK_AND_DEFINE(Slab*, slab, NewSlabMeta());
  EXPECT_EQ(slab, Heap().Start());
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MetadataManagerTest, AllocateSlabMetaTwice) {
  ASSERT_OK_AND_DEFINE(Slab*, s1, NewSlabMeta());
  ASSERT_THAT(FreeSlabMeta(s1), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());

  ASSERT_OK_AND_DEFINE(Slab*, s2, NewSlabMeta());
  EXPECT_EQ(s2, Heap().Start());
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MetadataManagerTest, AllocateSlabMetaWithNormalAllocation) {
  static_assert(sizeof(Slab) > 1, "Test only meaningful if sizeof(Slab) > 1");

  ASSERT_THAT(Alloc(kPageSize - 1).status(), IsOk());
  ASSERT_OK_AND_DEFINE(Slab*, s1, NewSlabMeta());
  EXPECT_THAT(ValidateHeap(), IsOk());
  // The new slab should have been placed at the beginning of the second page.
  EXPECT_EQ(s1, SlabManager().PageStartFromId(PageId(1)));
}

TEST_F(MetadataManagerTest, SlabMetaFreelistBeforeNewAlloc) {
  constexpr size_t kNumSlabs = 20;

  // Allocate a bunch of slabs.
  std::vector<Slab*> slabs;
  slabs.reserve(kNumSlabs);
  for (size_t i = 0; i < kNumSlabs; i++) {
    ASSERT_OK_AND_DEFINE(Slab*, slab, NewSlabMeta());
    ASSERT_THAT(ValidateHeap(), IsOk());
    slabs.push_back(slab);
  }

  // Free all of the slabs in random order.
  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(slabs.begin(), slabs.end(), rd);

  for (Slab* slab : slabs) {
    ASSERT_THAT(FreeSlabMeta(slab), IsOk());
  }

  // Allocate that many new slabs.
  std::vector<Slab*> new_slabs;
  new_slabs.reserve(kNumSlabs);
  for (size_t i = 0; i < kNumSlabs; i++) {
    ASSERT_OK_AND_DEFINE(Slab*, slab, NewSlabMeta());
    ASSERT_THAT(ValidateHeap(), IsOk());
    new_slabs.push_back(slab);
  }

  // Every new allocation should have come from a previous allocation.
  EXPECT_THAT(new_slabs, UnorderedElementsAreArray(slabs));
}

TEST_F(MetadataManagerTest, InterleaveSlabAllocAndAlloc) {
  ASSERT_OK_AND_DEFINE(Slab*, s1, NewSlabMeta());
  ASSERT_THAT(FreeSlabMeta(s1), IsOk());

  ASSERT_OK_AND_DEFINE(void*, value, Alloc(sizeof(Slab), alignof(Slab)));
  EXPECT_NE(value, s1);

  ASSERT_OK_AND_DEFINE(Slab*, s2, NewSlabMeta());
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(s2, s1);
}

}  // namespace ckmalloc

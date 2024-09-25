#include "src/ckmalloc/slab_map.h"

#include "absl/status/status.h"
#include "gtest/gtest.h"

#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/testlib.h"

namespace ckmalloc {

class SlabMapTest : public testing::Test, public CkMallocTest {
 public:
  static constexpr const char* kPrefix = "[SlabMapFixture]";

  TestSlabMap& SlabMap() {
    return slab_map_;
  }

  const char* TestPrefix() const override {
    return kPrefix;
  }

  absl::Status ValidateHeap() override {
    return absl::OkStatus();
  }

  bool AllocatePath(PageId start_id, PageId end_id);

 private:
  TestSlabMap slab_map_;
};

TEST_F(SlabMapTest, TestEmpty) {
  EXPECT_EQ(SlabMap().FindSlab(PageId(1000)), nullptr);
  EXPECT_EQ(TestGlobalMetadataAlloc::TotalAllocs(), 0);
}

TEST_F(SlabMapTest, TestInsertZero) {
  PageId page(0);
  MappedSlab* test_slab = reinterpret_cast<MappedSlab*>(0x1230);

  EXPECT_TRUE(SlabMap().AllocatePath(page, page));
  SlabMap().Insert(page, test_slab);
  EXPECT_EQ(SlabMap().FindSlab(page), test_slab);
  EXPECT_EQ(TestGlobalMetadataAlloc::TotalAllocs(), 4);
}

TEST_F(SlabMapTest, TestInsert) {
  MappedSlab* test_slab = reinterpret_cast<MappedSlab*>(0x5010203040);

  PageId id = PageId(12 + 2 * kNodeSize);
  EXPECT_TRUE(SlabMap().AllocatePath(id, id));
  SlabMap().Insert(id, test_slab);
  EXPECT_EQ(SlabMap().FindSlab(id), test_slab);

  EXPECT_EQ(SlabMap().FindSlab(PageId(0)), nullptr);
  EXPECT_EQ(SlabMap().FindSlab(PageId(2 * kNodeSize)), nullptr);
  EXPECT_EQ(TestGlobalMetadataAlloc::TotalAllocs(), 4);
}

TEST_F(SlabMapTest, TestAssignRange) {
  MappedSlab* test_slab = reinterpret_cast<MappedSlab*>(0x123456789abcd0);

  constexpr uint64_t kStartIdx = 20 + 10 * kNodeSize;
  constexpr uint64_t kEndIdx = 2 + 15 * kNodeSize;
  PageId start_id = PageId(kStartIdx);
  PageId end_id = PageId(kEndIdx);
  EXPECT_TRUE(SlabMap().AllocatePath(start_id, end_id));

  for (uint64_t i = kStartIdx; i <= kEndIdx; i++) {
    SlabMap().Insert(PageId(i), test_slab + (i - kStartIdx));
  }

  for (uint64_t i = kStartIdx - 1000; i <= kEndIdx + 1000; i++) {
    if (i >= kStartIdx && i <= kEndIdx) {
      ASSERT_EQ(SlabMap().FindSlab(PageId(i)), test_slab + (i - kStartIdx));
    } else {
      ASSERT_EQ(SlabMap().FindSlab(PageId(i)), nullptr);
    }
  }

  EXPECT_EQ(TestGlobalMetadataAlloc::TotalAllocs(), (6 + 1) * 2);
}

TEST_F(SlabMapTest, TestInsertRange) {
  MappedSlab* test_slab = reinterpret_cast<MappedSlab*>(0x123456789abcd0);

  constexpr uint64_t kStartIdx = 20 + 5 * kNodeSize;
  constexpr uint64_t kEndIdx = 2 + 15 * kNodeSize;
  PageId start_id = PageId(kStartIdx);
  PageId end_id = PageId(kEndIdx);
  EXPECT_TRUE(SlabMap().AllocatePath(start_id, end_id));
  SlabMap().InsertRange(start_id, end_id, test_slab);

  for (uint64_t i = kStartIdx - 1000; i <= kEndIdx + 1000; i++) {
    if (i >= kStartIdx && i <= kEndIdx) {
      ASSERT_EQ(SlabMap().FindSlab(PageId(i)), test_slab);
    } else {
      ASSERT_EQ(SlabMap().FindSlab(PageId(i)), nullptr);
    }
  }

  EXPECT_EQ(TestGlobalMetadataAlloc::TotalAllocs(), (11 + 1) * 2);
}

TEST_F(SlabMapTest, TestInsertLongRange) {
  MappedSlab* test_slab = reinterpret_cast<MappedSlab*>(0x123456789abcd0);

  constexpr uint64_t kStartIdx = 20 + 5 * kNodeSize + 3 * kNodeSize * kNodeSize;
  constexpr uint64_t kEndIdx = 2 + 15 * kNodeSize + 5 * kNodeSize * kNodeSize;
  PageId start_id = PageId(kStartIdx);
  PageId end_id = PageId(kEndIdx);
  EXPECT_TRUE(SlabMap().AllocatePath(start_id, end_id));
  SlabMap().InsertRange(start_id, end_id, test_slab);

  for (uint64_t i = kStartIdx - 1000; i <= kEndIdx + 1000; i++) {
    if (i >= kStartIdx && i <= kEndIdx) {
      ASSERT_EQ(SlabMap().FindSlab(PageId(i)), test_slab);
    } else {
      ASSERT_EQ(SlabMap().FindSlab(PageId(i)), nullptr);
    }
  }

  EXPECT_EQ(TestGlobalMetadataAlloc::TotalAllocs(),
            ((2 * kNodeSize + 11) + 3) * 2);
}

TEST_F(SlabMapTest, TestDeallocate) {
  PageId id = PageId(88 + 400 * kNodeSize + 123 * kNodeSize * kNodeSize);
  EXPECT_TRUE(SlabMap().AllocatePath(id, id));
  SlabMap().DeallocatePath(id, id);

  EXPECT_EQ(TestGlobalMetadataAlloc::TotalAllocs(), 4);
}

TEST_F(SlabMapTest, TestReallocate) {
  PageId id1 = PageId(200 + 400 * kNodeSize + 600 * kNodeSize * kNodeSize);
  EXPECT_TRUE(SlabMap().AllocatePath(id1, id1));
  SlabMap().DeallocatePath(id1, id1);

  PageId id2 = PageId(100 + 200 * kNodeSize + 300 * kNodeSize * kNodeSize);
  EXPECT_TRUE(SlabMap().AllocatePath(id2, id2));

  // The second AllocatePath should not require any more mallocs since it can
  // reuse the just-deleted nodes.
  EXPECT_EQ(TestGlobalMetadataAlloc::TotalAllocs(), 4);
}

TEST_F(SlabMapTest, TestReallocateMultiple) {
  PageId id1 = PageId(200 + 400 * kNodeSize + 10 * kNodeSize * kNodeSize);
  PageId id2 = PageId(100 + 500 * kNodeSize + 10 * kNodeSize * kNodeSize);
  EXPECT_TRUE(SlabMap().AllocatePath(id1, id2));

  PageId id3 = PageId(300 + 410 * kNodeSize + 10 * kNodeSize * kNodeSize);
  PageId id4 = PageId(500 + 450 * kNodeSize + 10 * kNodeSize * kNodeSize);
  SlabMap().DeallocatePath(id3, id4);

  PageId id5 = PageId(200 + 400 * kNodeSize + 10 * kNodeSize * kNodeSize);
  PageId id6 = PageId(250 + 410 * kNodeSize + 10 * kNodeSize * kNodeSize);
  SlabMap().DeallocatePath(id5, id6);

  PageId id7 = PageId(251 + 410 * kNodeSize + 10 * kNodeSize * kNodeSize);
  PageId id8 = PageId(297 + 410 * kNodeSize + 10 * kNodeSize * kNodeSize);
  SlabMap().DeallocatePath(id7, id8);

  PageId id9 = PageId(298 + 410 * kNodeSize + 10 * kNodeSize * kNodeSize);
  PageId id10 = PageId(299 + 410 * kNodeSize + 10 * kNodeSize * kNodeSize);
  SlabMap().DeallocatePath(id9, id10);

  // The second AllocatePath should not require any more mallocs since it can
  // reuse the just-deleted nodes.
  EXPECT_EQ(TestGlobalMetadataAlloc::TotalAllocs(), (101 + 1) * 2);

  // We should be able to allocate 50 nodes worth without incurring any
  // additional allocations.
  PageId id11 = PageId(0 + 0 * kNodeSize + 10 * kNodeSize * kNodeSize);
  PageId id12 = PageId(0 + 49 * kNodeSize + 10 * kNodeSize * kNodeSize);
  EXPECT_TRUE(SlabMap().AllocatePath(id11, id12));
  EXPECT_EQ(TestGlobalMetadataAlloc::TotalAllocs(), (101 + 1) * 2);
}

}  // namespace ckmalloc

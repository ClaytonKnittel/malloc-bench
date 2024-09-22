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

 private:
  TestSlabMap slab_map_;
};

TEST_F(SlabMapTest, TestEmpty) {
  EXPECT_EQ(SlabMap().FindSlab(PageId(1000)), nullptr);
}

TEST_F(SlabMapTest, TestInsertZero) {
  PageId page(1024);
  MappedSlab* test_slab = reinterpret_cast<MappedSlab*>(0x1230);

  EXPECT_TRUE(SlabMap().AllocatePath(page, page));
  SlabMap().Insert(page, test_slab);
  EXPECT_EQ(SlabMap().FindSlab(page), test_slab);
}

TEST_F(SlabMapTest, TestInsert) {
  MappedSlab* test_slab = reinterpret_cast<MappedSlab*>(0x5010203040);

  PageId id = PageId(12 + 2 * kLeafSize);
  EXPECT_TRUE(SlabMap().AllocatePath(id, id));
  SlabMap().Insert(id, test_slab);
  EXPECT_EQ(SlabMap().FindSlab(id), test_slab);

  EXPECT_EQ(SlabMap().FindSlab(PageId(0)), nullptr);
  EXPECT_EQ(SlabMap().FindSlab(PageId(2 * kLeafSize)), nullptr);
}

TEST_F(SlabMapTest, TestAssignRange) {
  MappedSlab* test_slab = reinterpret_cast<MappedSlab*>(0x123456789abcd0);

  constexpr uint32_t kStartIdx = 20 + 10 * kLeafSize;
  constexpr uint32_t kEndIdx = 2 + 15 * kLeafSize;
  PageId start_id = PageId(kStartIdx);
  PageId end_id = PageId(kEndIdx);
  EXPECT_TRUE(SlabMap().AllocatePath(start_id, end_id));

  for (uint32_t i = kStartIdx; i <= kEndIdx; i++) {
    SlabMap().Insert(PageId(i), test_slab + (i - kStartIdx));
  }

  for (uint32_t i = kStartIdx - 1000; i <= kEndIdx + 1000; i++) {
    if (i >= kStartIdx && i <= kEndIdx) {
      ASSERT_EQ(SlabMap().FindSlab(PageId(i)), test_slab + (i - kStartIdx));
    } else {
      ASSERT_EQ(SlabMap().FindSlab(PageId(i)), nullptr);
    }
  }
}

TEST_F(SlabMapTest, TestInsertRange) {
  MappedSlab* test_slab = reinterpret_cast<MappedSlab*>(0x123456789abcd0);

  constexpr uint32_t kStartIdx = 20 + 5 * kLeafSize;
  constexpr uint32_t kEndIdx = 2 + 15 * kLeafSize;
  PageId start_id = PageId(kStartIdx);
  PageId end_id = PageId(kEndIdx);
  EXPECT_TRUE(SlabMap().AllocatePath(start_id, end_id));
  SlabMap().InsertRange(start_id, end_id, test_slab);

  for (uint32_t i = kStartIdx - 1000; i <= kEndIdx + 1000; i++) {
    if (i >= kStartIdx && i <= kEndIdx) {
      ASSERT_EQ(SlabMap().FindSlab(PageId(i)), test_slab);
    } else {
      ASSERT_EQ(SlabMap().FindSlab(PageId(i)), nullptr);
    }
  }
}

}  // namespace ckmalloc

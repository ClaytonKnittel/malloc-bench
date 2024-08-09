#include "src/ckmalloc/slab_map.h"

#include "gtest/gtest.h"
#include "util/gtest_util.h"

#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/testlib.h"

namespace ckmalloc {

using util::IsOk;

class SlabMapTest : public CkMallocTest {
 public:
  TestSlabMap& SlabMap() {
    return slab_map_;
  }

 private:
  TestSlabMap slab_map_;
};

TEST_F(SlabMapTest, TestEmpty) {
  EXPECT_EQ(SlabMap().FindSlab(PageId::Zero()), nullptr);
}

TEST_F(SlabMapTest, TestInsertZero) {
  Slab* test_slab = reinterpret_cast<Slab*>(0x1230);

  EXPECT_THAT(SlabMap().AllocatePath(PageId::Zero(), PageId::Zero()), IsOk());
  SlabMap().Insert(PageId::Zero(), test_slab);
  EXPECT_EQ(SlabMap().FindSlab(PageId::Zero()), test_slab);
}

TEST_F(SlabMapTest, TestInsert) {
  Slab* test_slab = reinterpret_cast<Slab*>(0x5010203040);

  PageId id = PageId::Zero() + 12 + 2 * kNodeSize + 5 * kNodeSize * kNodeSize;
  EXPECT_THAT(SlabMap().AllocatePath(id, id), IsOk());
  SlabMap().Insert(id, test_slab);
  EXPECT_EQ(SlabMap().FindSlab(id), test_slab);

  EXPECT_EQ(SlabMap().FindSlab(PageId::Zero()), nullptr);
  EXPECT_EQ(SlabMap().FindSlab(PageId::Zero() + 5 * kNodeSize * kNodeSize),
            nullptr);
  EXPECT_EQ(SlabMap().FindSlab(PageId::Zero() + 2 * kNodeSize +
                               5 * kNodeSize * kNodeSize),
            nullptr);
}

TEST_F(SlabMapTest, TestAssignRange) {
  Slab* test_slab = reinterpret_cast<Slab*>(0x123456789abcd0);

  constexpr uint32_t kStartIdx =
      20 + 5 * kNodeSize + 16 * kNodeSize * kNodeSize;
  constexpr uint32_t kEndIdx = 2 + 10 * kNodeSize + 18 * kNodeSize * kNodeSize;
  PageId start_id = PageId::Zero() + kStartIdx;
  PageId end_id = PageId::Zero() + kEndIdx;
  EXPECT_THAT(SlabMap().AllocatePath(start_id, end_id), IsOk());

  for (uint32_t i = kStartIdx; i <= kEndIdx; i++) {
    SlabMap().Insert(PageId::Zero() + i, test_slab + (i - kStartIdx));
  }

  for (uint32_t i = kStartIdx - 1000; i <= kEndIdx + 1000; i++) {
    if (i >= kStartIdx && i <= kEndIdx) {
      ASSERT_EQ(SlabMap().FindSlab(PageId::Zero() + i),
                test_slab + (i - kStartIdx));
    } else {
      ASSERT_EQ(SlabMap().FindSlab(PageId::Zero() + i), nullptr);
    }
  }
}

TEST_F(SlabMapTest, TestInsertRange) {
  Slab* test_slab = reinterpret_cast<Slab*>(0x123456789abcd0);

  constexpr uint32_t kStartIdx =
      20 + 5 * kNodeSize + 16 * kNodeSize * kNodeSize;
  constexpr uint32_t kEndIdx = 2 + 10 * kNodeSize + 18 * kNodeSize * kNodeSize;
  PageId start_id = PageId::Zero() + kStartIdx;
  PageId end_id = PageId::Zero() + kEndIdx;
  EXPECT_THAT(SlabMap().AllocatePath(start_id, end_id), IsOk());
  SlabMap().InsertRange(start_id, end_id, test_slab);

  for (uint32_t i = kStartIdx - 1000; i <= kEndIdx + 1000; i++) {
    if (i >= kStartIdx && i <= kEndIdx) {
      ASSERT_EQ(SlabMap().FindSlab(PageId::Zero() + i), test_slab);
    } else {
      ASSERT_EQ(SlabMap().FindSlab(PageId::Zero() + i), nullptr);
    }
  }
}

}  // namespace ckmalloc
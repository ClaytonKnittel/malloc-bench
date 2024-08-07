#include "src/ckmalloc/slab_map.h"

#include <new>

#include "gtest/gtest.h"
#include "util/gtest_util.h"

#include "src/ckmalloc/slab_id.h"
#include "src/ckmalloc/testlib.h"

namespace ckmalloc {

using util::IsOk;

class SlabMapTest : public ::testing::Test {
 public:
  ~SlabMapTest() override {
    FreeSlabMap();
  }

  TestSlabMap& SlabMap() {
    return slab_map_;
  }

 private:
  static void FreeLeaf(TestSlabMap::Leaf* leaf) {
    ::operator delete(
        leaf, static_cast<std::align_val_t>(alignof(TestSlabMap::Leaf)));
  }

  static void FreeNode(TestSlabMap::Node* node) {
    for (TestSlabMap::Leaf* leaf : node->leaves_) {
      if (leaf != nullptr) {
        FreeLeaf(leaf);
      }
    }
    ::operator delete(
        node, static_cast<std::align_val_t>(alignof(TestSlabMap::Node)));
  }

  void FreeSlabMap() {
    for (TestSlabMap::Node* node : slab_map_.nodes_) {
      if (node != nullptr) {
        FreeNode(node);
      }
    }
  }

  TestSlabMap slab_map_;
};

TEST_F(SlabMapTest, TestEmpty) {
  EXPECT_EQ(SlabMap().FindSlab(SlabId::Zero()), nullptr);
}

TEST_F(SlabMapTest, TestInsertZero) {
  Slab* test_slab = reinterpret_cast<Slab*>(0x1230);

  EXPECT_THAT(SlabMap().AllocatePath(SlabId::Zero(), SlabId::Zero()), IsOk());
  SlabMap().Insert(SlabId::Zero(), test_slab);
  EXPECT_EQ(SlabMap().FindSlab(SlabId::Zero()), test_slab);
}

TEST_F(SlabMapTest, TestInsert) {
  Slab* test_slab = reinterpret_cast<Slab*>(0x5010203040);

  SlabId id = SlabId::Zero() + 12 + 2 * kNodeSize + 5 * kNodeSize * kNodeSize;
  EXPECT_THAT(SlabMap().AllocatePath(id, id), IsOk());
  SlabMap().Insert(id, test_slab);
  EXPECT_EQ(SlabMap().FindSlab(id), test_slab);

  EXPECT_EQ(SlabMap().FindSlab(SlabId::Zero()), nullptr);
  EXPECT_EQ(SlabMap().FindSlab(SlabId::Zero() + 5 * kNodeSize * kNodeSize),
            nullptr);
  EXPECT_EQ(SlabMap().FindSlab(SlabId::Zero() + 2 * kNodeSize +
                               5 * kNodeSize * kNodeSize),
            nullptr);
}

TEST_F(SlabMapTest, TestAssignRange) {
  Slab* test_slab = reinterpret_cast<Slab*>(0x123456789abcd0);

  constexpr uint32_t kStartIdx =
      20 + 5 * kNodeSize + 16 * kNodeSize * kNodeSize;
  constexpr uint32_t kEndIdx = 2 + 10 * kNodeSize + 18 * kNodeSize * kNodeSize;
  SlabId start_id = SlabId::Zero() + kStartIdx;
  SlabId end_id = SlabId::Zero() + kEndIdx;
  EXPECT_THAT(SlabMap().AllocatePath(start_id, end_id), IsOk());

  for (uint32_t i = kStartIdx; i <= kEndIdx; i++) {
    SlabMap().Insert(SlabId::Zero() + i, test_slab + (i - kStartIdx));
  }

  for (uint32_t i = kStartIdx - 1000; i <= kEndIdx + 1000; i++) {
    if (i >= kStartIdx && i <= kEndIdx) {
      ASSERT_EQ(SlabMap().FindSlab(SlabId::Zero() + i),
                test_slab + (i - kStartIdx));
    } else {
      ASSERT_EQ(SlabMap().FindSlab(SlabId::Zero() + i), nullptr);
    }
  }
}

TEST_F(SlabMapTest, TestInsertRange) {
  Slab* test_slab = reinterpret_cast<Slab*>(0x123456789abcd0);

  constexpr uint32_t kStartIdx =
      20 + 5 * kNodeSize + 16 * kNodeSize * kNodeSize;
  constexpr uint32_t kEndIdx = 2 + 10 * kNodeSize + 18 * kNodeSize * kNodeSize;
  SlabId start_id = SlabId::Zero() + kStartIdx;
  SlabId end_id = SlabId::Zero() + kEndIdx;
  EXPECT_THAT(SlabMap().AllocatePath(start_id, end_id), IsOk());
  SlabMap().InsertRange(start_id, end_id, test_slab);

  for (uint32_t i = kStartIdx - 1000; i <= kEndIdx + 1000; i++) {
    if (i >= kStartIdx && i <= kEndIdx) {
      ASSERT_EQ(SlabMap().FindSlab(SlabId::Zero() + i), test_slab);
    } else {
      ASSERT_EQ(SlabMap().FindSlab(SlabId::Zero() + i), nullptr);
    }
  }
}

}  // namespace ckmalloc

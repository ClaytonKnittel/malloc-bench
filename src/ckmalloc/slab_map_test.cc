#include "src/ckmalloc/slab_map.h"

#include <new>

#include "gtest/gtest.h"
#include "util/gtest_util.h"

#include "src/ckmalloc/slab_id.h"

namespace ckmalloc {

using util::IsOk;

void* Allocate(size_t size, size_t alignment) {
  return ::operator new(size, static_cast<std::align_val_t>(alignment));
}

using TestSlabMap = SlabMapImpl<Allocate>;

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

}  // namespace ckmalloc

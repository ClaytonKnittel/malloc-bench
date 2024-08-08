#include "src/ckmalloc/slab_manager.h"

#include "gtest/gtest.h"

#include "src/ckmalloc/slab_id.h"
#include "src/ckmalloc/testlib.h"

namespace ckmalloc {

class SlabManagerTest : public ::testing::Test {
 public:
  // TODO: either make slab_map virtual in slab manager, or template slab map.
  SlabManagerTest() : heap_(64), slab_manager_(&heap_, &slab_map_) {}

  TestHeap& Heap() {
    return heap_;
  }

  TestSlabMap& SlabMap() {
    return slab_map_;
  }

  TestSlabManager& SlabManager() {
    return slab_manager_;
  }

 private:
  TestHeap heap_;
  TestSlabMap slab_map_;
  TestSlabManager slab_manager_;
};

TEST_F(SlabManagerTest, HeapStartIsSlabIdZero) {
  SlabManager().Alloc(1);
  EXPECT_EQ(SlabManager().SlabIdFromPtr(Heap().Start()), SlabId(0));
}

}  // namespace ckmalloc

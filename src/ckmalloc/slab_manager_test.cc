#include "src/ckmalloc/slab_manager.h"

#include "gtest/gtest.h"

#include "src/ckmalloc/testlib.h"

namespace ckmalloc {

class SlabManagerTest : public ::testing::Test {
 public:
  // TODO: either make slab_map virtual in slab manager, or template slab map.
  SlabManagerTest() : heap_(64), slab_manager_(&heap_, &slab_map_) {}

 private:
  TestHeap heap_;
  TestSlabMap slab_map_;
  SlabManager slab_manager_;
};

TEST_F(SlabManagerTest, TestEmpty) {}

}  // namespace ckmalloc

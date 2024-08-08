#include "src/ckmalloc/slab_manager.h"

#include <cstddef>

#include "gtest/gtest.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/slab_id.h"
#include "src/ckmalloc/testlib.h"

namespace ckmalloc {

class SlabManagerTest : public CkMallocTest {
 public:
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

TEST_F(SlabManagerTest, AllPtrsInFirstSlabIdZero) {
  SlabManager().Alloc(1);
  for (size_t offset = 0; offset < kPageSize; offset++) {
    EXPECT_EQ(SlabManager().SlabIdFromPtr(
                  static_cast<uint8_t*>(Heap().Start()) + offset),
              SlabId(0));
  }
}

TEST_F(SlabManagerTest, SlabIdIncreasesPerPage) {
  constexpr size_t kPages = 16;
  SlabManager().Alloc(kPages);
  for (size_t page_n = 0; page_n < kPages; page_n++) {
    void* beginning =
        static_cast<uint8_t*>(Heap().Start()) + page_n * kPageSize;
    void* end = static_cast<uint8_t*>(beginning) + kPageSize - 1;
    EXPECT_EQ(SlabManager().SlabIdFromPtr(beginning), SlabId(page_n));
    EXPECT_EQ(SlabManager().SlabIdFromPtr(end), SlabId(page_n));
  }
}

TEST_F(SlabManagerTest, SlabStartFromId) {
  constexpr size_t kPages = 16;
  SlabManager().Alloc(kPages);
  for (size_t page_n = 0; page_n < kPages; page_n++) {
    EXPECT_EQ(SlabManager().SlabStartFromId(SlabId(page_n)),
              static_cast<uint8_t*>(Heap().Start()) + page_n * kPageSize);
  }
}

}  // namespace ckmalloc

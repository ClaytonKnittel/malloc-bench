#include "src/jsmalloc/blocks/small_block.h"

#include <cstddef>
#include <vector>

#include "gtest/gtest.h"

#include "src/jsmalloc/allocator.h"
#include "src/jsmalloc/blocks/free_block_allocator.h"
#include "src/jsmalloc/blocks/sentinel_block_allocator.h"

namespace jsmalloc {
namespace blocks {

class SmallBlockTest : public ::testing::Test {
 public:
  void SetUp() override {
    sentinel_heap.Init();
  }

  SmallBlock* New(size_t data_size, size_t bin_count) {
    auto* block = free_block_allocator.Allocate(
        SmallBlock::RequiredSize(data_size, bin_count));
    if (block == nullptr)
      return nullptr;
    return SmallBlock::Init(block, data_size, bin_count);
  }

  jsmalloc::testing::TestRegionAllocator region_allocator;
  SentinelBlockHeap sentinel_heap =
      SentinelBlockHeap(&region_allocator, &region_allocator);
  FreeBlockAllocator free_block_allocator = FreeBlockAllocator(sentinel_heap);
};

TEST_F(SmallBlockTest, AllocAndFree) {
  size_t bin_count = 400;
  auto* block = New(8, bin_count);
  EXPECT_TRUE(block->IsEmpty());

  std::vector<void*> ptrs;
  while (!block->IsFull()) {
    ptrs.push_back(block->Alloc());
  }

  EXPECT_EQ(ptrs.size(), bin_count);

  for (void* ptr : ptrs) {
    EXPECT_FALSE(block->IsEmpty());
    block->Free(ptr);
    EXPECT_TRUE(!block->IsFull());
  }

  EXPECT_TRUE(block->IsEmpty());
}

}  // namespace blocks
}  // namespace jsmalloc

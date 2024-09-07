#include "src/jsmalloc/blocks/small_block.h"

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

  jsmalloc::testing::TestHeap heap;
  SentinelBlockHeap sentinel_heap = SentinelBlockHeap(heap);
  FreeBlockAllocator free_block_allocator = FreeBlockAllocator(sentinel_heap);
};

TEST_F(SmallBlockTest, FullLifecycle) {
  SmallBlock* block = SmallBlock::New(free_block_allocator, 12, 32);

  EXPECT_TRUE(block->IsEmpty());

  std::vector<void*> ptrs;
  while (!block->IsFull()) {
    ptrs.push_back(block->Alloc());
  }

  EXPECT_EQ(ptrs.size(), 32);

  for (void* ptr : ptrs) {
    EXPECT_FALSE(block->IsEmpty());
    block->Free(ptr);
    EXPECT_TRUE(!block->IsFull());
  }

  EXPECT_TRUE(block->IsEmpty());
}

TEST_F(SmallBlockTest, ReportsSize) {
  SmallBlock* block = SmallBlock::New(free_block_allocator, 12, 32);

  EXPECT_GT(block->BlockSize(), 12 * 32);
  EXPECT_EQ(block->DataSize(), 12);
}

TEST_F(SmallBlockTest, FromDataPointer) {
  SmallBlock* block = SmallBlock::New(free_block_allocator, 12, 20);

  std::vector<void*> ptrs;
  while (!block->IsFull()) {
    ptrs.push_back(block->Alloc());
  }

  EXPECT_EQ(ptrs.size(), 20);

  for (void* ptr : ptrs) {
    EXPECT_EQ(static_cast<void*>(BlockHeader::FromDataPtr(ptr)),
              static_cast<void*>(block));
  }
}

}  // namespace blocks
}  // namespace jsmalloc

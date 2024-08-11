#include "src/jsmalloc/blocks/small_block.h"

#include <vector>

#include "gtest/gtest.h"

#include "src/jsmalloc/allocator.h"
#include "src/jsmalloc/blocks/free_block_allocator.h"

namespace jsmalloc {
namespace blocks {

TEST(TestSmallBlock, FullLifecycle) {
  BigStackAllocator allocator;
  FreeBlockAllocator free_block_allocator(allocator);
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

TEST(TestSmallBlock, ReportsSize) {
  BigStackAllocator allocator;
  FreeBlockAllocator free_block_allocator(allocator);
  SmallBlock* block = SmallBlock::New(free_block_allocator, 12, 32);

  EXPECT_GT(block->BlockSize(), 12 * 32);
  EXPECT_EQ(block->DataSize(), 12);
}

TEST(TestSmallBlock, FromDataPointer) {
  BigStackAllocator allocator;
  FreeBlockAllocator free_block_allocator(allocator);
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

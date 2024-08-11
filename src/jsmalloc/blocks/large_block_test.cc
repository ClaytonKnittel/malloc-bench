#include "src/jsmalloc/blocks/large_block.h"

#include "gtest/gtest.h"

#include "src/jsmalloc/allocator.h"
#include "src/jsmalloc/blocks/block.h"

namespace jsmalloc {
namespace blocks {

TEST(TestLargeBlock, FromDataPtr) {
  BigStackAllocator allocator;
  LargeBlock* block = LargeBlock::New(allocator, 50);
  void* ptr = block->Data();
  auto* block_from_ptr = reinterpret_cast<LargeBlock*>(BlockHeader::FromDataPtr(ptr));
  EXPECT_EQ(block, block_from_ptr);
}

TEST(TestLargeBlock, ComputesSize) {
  BigStackAllocator allocator;
  LargeBlock* large_block = LargeBlock::New(allocator, 100);
  auto* block = reinterpret_cast<BlockHeader*>(large_block);

  EXPECT_GT(block->BlockSize(), 100);
  EXPECT_LT(block->BlockSize(), 200);
}

}  // namespace blocks
}  // namespace jsmalloc

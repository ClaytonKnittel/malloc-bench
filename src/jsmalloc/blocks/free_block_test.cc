#include "src/jsmalloc/blocks/free_block.h"

#include "gtest/gtest.h"

#include "src/jsmalloc/allocator.h"

namespace jsmalloc {
namespace blocks {

TEST(TestFreeBlock, AllowsNopResize) {
  BigStackAllocator allocator;

  FreeBlock* block = FreeBlock::New(allocator, 48);
  EXPECT_TRUE(block->CanResizeTo(48));

  FreeBlock* remainder = block->ResizeTo(48);
  EXPECT_EQ(remainder, nullptr);
}

TEST(TestFreeBlock, AllowsSplitting) {
  BigStackAllocator allocator;

  FreeBlock* block = FreeBlock::New(allocator, 128);
  EXPECT_TRUE(block->CanResizeTo(48));

  FreeBlock* remainder = block->ResizeTo(48);
  EXPECT_EQ(block->BlockSize(), 48);
  EXPECT_EQ(remainder->BlockSize(), 128 - 48);
}

TEST(TestFreeBlock, ResizeRejectsLargerSizes) {
  BigStackAllocator allocator;

  FreeBlock* block = FreeBlock::New(allocator, 128);
  EXPECT_FALSE(block->CanResizeTo(256));
}

}  // namespace blocks
}  // namespace jsmalloc

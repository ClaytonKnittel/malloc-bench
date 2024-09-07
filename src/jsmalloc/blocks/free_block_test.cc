#include "src/jsmalloc/blocks/free_block.h"

#include "gtest/gtest.h"

namespace jsmalloc {
namespace blocks {

class FreeBlockTest : public ::testing::Test {
 public:
  void SetUp() override {
    sentinel_heap.Init();
  }

  jsmalloc::testing::TestHeap heap;
  SentinelBlockHeap sentinel_heap = SentinelBlockHeap(heap);
};

TEST_F(FreeBlockTest, AllowsNopResize) {
  FreeBlock* block = FreeBlock::New(sentinel_heap, 48);
  EXPECT_TRUE(block->CanMarkUsed(48));

  FreeBlock* remainder = block->MarkUsed(48);
  EXPECT_EQ(remainder, nullptr);
}

TEST_F(FreeBlockTest, AllowsSplitting) {
  FreeBlock* block = FreeBlock::New(sentinel_heap, 128);
  EXPECT_TRUE(block->CanMarkUsed(48));

  FreeBlock* remainder = block->MarkUsed(48);
  EXPECT_EQ(block->BlockSize(), 48);
  EXPECT_EQ(remainder->BlockSize(), 128 - 48);
}

TEST_F(FreeBlockTest, ResizeRejectsLargerSizes) {
  FreeBlock* block = FreeBlock::New(sentinel_heap, 128);
  EXPECT_FALSE(block->CanMarkUsed(256));
}

}  // namespace blocks
}  // namespace jsmalloc

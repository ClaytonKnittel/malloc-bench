#include "src/jsmalloc/blocks/large_block.h"

#include "gtest/gtest.h"

#include "src/jsmalloc/allocator.h"
#include "src/jsmalloc/blocks/block.h"
#include "src/jsmalloc/blocks/free_block_allocator.h"
#include "src/jsmalloc/blocks/sentinel_block_allocator.h"

namespace jsmalloc {
namespace blocks {

class LargeBlockTest : public ::testing::Test {
 public:
  void SetUp() override {
    sentinel_heap.Init();
  }

  jsmalloc::testing::TestHeap heap;
  SentinelBlockHeap sentinel_heap = SentinelBlockHeap(heap);
  FreeBlockAllocator free_block_allocator = FreeBlockAllocator(sentinel_heap);
};

TEST_F(LargeBlockTest, FromDataPtr) {
  LargeBlock* block = LargeBlock::Init(
      free_block_allocator.Allocate(LargeBlock::BlockSize(50)));
  void* ptr = block->Data();
  auto* block_from_ptr =
      reinterpret_cast<LargeBlock*>(BlockHeader::FromDataPtr(ptr));
  EXPECT_EQ(block, block_from_ptr);
}

TEST_F(LargeBlockTest, ComputesSize) {
  LargeBlock* large_block = LargeBlock::Init(
      free_block_allocator.Allocate(LargeBlock::BlockSize(100)));
  auto* block = reinterpret_cast<BlockHeader*>(large_block);

  EXPECT_GT(block->BlockSize(), 100);
  EXPECT_LT(block->BlockSize(), 200);
}

}  // namespace blocks
}  // namespace jsmalloc

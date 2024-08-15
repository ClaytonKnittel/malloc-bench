#include "src/ckmalloc/block.h"

#include <cstdint>

#include "gtest/gtest.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

class BlockTest : public ::testing::Test {
 public:
  static bool PrevFree(const Block* block) {
    return block->PrevFree();
  }

  static void SetSize(Block* block, uint64_t size) {
    block->SetSize(size);
  }
};

TEST_F(BlockTest, UserToBlockSize) {
  EXPECT_EQ(Block::BlockSizeForUserSize(kMaxSmallSize + 1),
            Block::kMinLargeSize);
  EXPECT_EQ(Block::BlockSizeForUserSize(Block::kMinLargeSize -
                                        Block::kMetadataOverhead),
            Block::kMinLargeSize);
  EXPECT_EQ(Block::BlockSizeForUserSize(Block::kMinLargeSize -
                                        Block::kMetadataOverhead + 1),
            Block::kMinLargeSize + kDefaultAlignment);
}

TEST_F(BlockTest, AllocatedBlock) {
  constexpr size_t kBlockSize = 0xabcdef0;
  Block block;

  block.InitAllocated(kBlockSize, /*prev_free=*/false);
  EXPECT_FALSE(block.Free());
  EXPECT_EQ(block.Size(), kBlockSize);
  EXPECT_FALSE(PrevFree(&block));

  EXPECT_EQ(PtrDistance(block.NextAdjacentBlock(), &block), kBlockSize);

  EXPECT_EQ(block.UserDataSize(), kBlockSize - Block::kMetadataOverhead);
  EXPECT_FALSE(block.IsUntracked());

  AllocatedBlock* allocated = block.ToAllocated();
  EXPECT_EQ(allocated->UserDataPtr(),
            PtrAdd<void>(&block, Block::kMetadataOverhead));
  EXPECT_EQ(AllocatedBlock::FromUserDataPtr(allocated->UserDataPtr()), &block);
}

}  // namespace ckmalloc

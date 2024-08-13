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

  static void WriteFooterAndPrevFree(Block* block) {
    block->WriteFooterAndPrevFree();
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
  EXPECT_FALSE(block.IsUntrackedSize());

  AllocatedBlock* allocated = block.ToAllocated();
  EXPECT_EQ(allocated->UserDataPtr(),
            reinterpret_cast<uint8_t*>(&block) + Block::kMetadataOverhead);
  EXPECT_EQ(AllocatedBlock::FromUserDataPtr(allocated->UserDataPtr()), &block);
}

TEST_F(BlockTest, UntrackedBlock) {
  constexpr size_t kBlockSize = 0x10;
  Block block;

  block.InitUntracked(kBlockSize);
  EXPECT_TRUE(block.Free());
  EXPECT_EQ(block.Size(), kBlockSize);
  EXPECT_FALSE(PrevFree(&block));

  EXPECT_EQ(PtrDistance(block.NextAdjacentBlock(), &block), kBlockSize);

  EXPECT_TRUE(block.IsUntrackedSize());

  // This should not cause assertion failure.
  block.ToUntracked();
}

TEST_F(BlockTest, PrevFree) {
  constexpr size_t kBlockSize = 0x1030;
  uint64_t
      region[(kBlockSize + Block::kMetadataOverhead) / sizeof(uint64_t)] = {};
  Block* block =
      reinterpret_cast<Block*>(&region[kBlockSize / sizeof(uint64_t)]);
  Block* prev = reinterpret_cast<Block*>(&region[0]);

  block->InitAllocated(0x54540, /*prev_free=*/false);

  // First initialize the block as untracked, which is a type of free block,
  // then artificially increase its size to the desired size. This has to be
  // done because we can't directly initialize a free block (this requires
  // insertion into a freelist), and `InitUntracked` checks that the size is
  // below the min large block size.
  prev->InitUntracked(0x10);
  SetSize(prev, kBlockSize);

  // This should write to the footer of `block`, and set block's prev_free bit.
  WriteFooterAndPrevFree(prev);

  EXPECT_TRUE(PrevFree(block));
  EXPECT_EQ(block->PrevAdjacentBlock(), prev);
}

}  // namespace ckmalloc

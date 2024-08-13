#include "src/ckmalloc/block.h"

#include "gtest/gtest.h"

#include "src/ckmalloc/common.h"

namespace ckmalloc {

class BlockTest : public ::testing::Test {};

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

}  // namespace ckmalloc

#include "src/jsmalloc/blocks/block.h"

#include "gtest/gtest.h"

namespace jsmalloc {
namespace blocks {

TEST(TestBlockHeader, Initialization) {
  BlockHeader header(128, BlockKind::kSmall, false);

  EXPECT_EQ(header.BlockSize(), 128);
  EXPECT_EQ(header.Kind(), BlockKind::kSmall);
  EXPECT_EQ(header.PrevBlockIsFree(), false);
}

TEST(TestBlockHeader, LargeData) {
  uint32_t large_size = (1 << 29) - 1;
  large_size &= ~0b1111;
  BlockKind kind = BlockKind::kBeginOrEnd;

  BlockHeader header(large_size, kind, true);

  EXPECT_EQ(header.BlockSize(), large_size);
  EXPECT_EQ(header.Kind(), kind);
  EXPECT_EQ(header.PrevBlockIsFree(), true);
}

}  // namespace blocks
}  // namespace jsmalloc

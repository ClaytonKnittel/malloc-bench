#include "src/jsmalloc/blocks/block.h"

#include "gtest/gtest.h"

namespace jsmalloc {
namespace blocks {

TEST(TestBlockHeader, Initialization) {
  BlockHeader header(120, BlockKind::kSmall);

  EXPECT_EQ(header.BlockSize(), 120);
  EXPECT_EQ(header.Kind(), BlockKind::kSmall);
}

TEST(TestBlockHeader, LargeData) {
  uint32_t large_size = (1 << 29) - 1;
  large_size &= ~0b111;
  BlockKind kind = BlockKind::kBeginOrEnd;

  BlockHeader header(large_size, kind);

  EXPECT_EQ(header.BlockSize(), large_size);
  EXPECT_EQ(header.Kind(), kind);
}

}  // namespace blocks
}  // namespace jsmalloc

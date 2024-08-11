#include "src/jsmalloc/blocks/block.h"

#include "gtest/gtest.h"

namespace jsmalloc {
namespace blocks {

TEST(TestBlockHeader, Initialization) {
  BlockHeader metadata(120, BlockKind::kSmall);

  EXPECT_EQ(metadata.BlockSize(), 120);
  EXPECT_EQ(metadata.Kind(), BlockKind::kSmall);
}

TEST(TestBlockHeader, LargeData) {
  uint32_t large_size = (1 << 29) - 1;
  large_size &= ~0b111;
  BlockKind kind = BlockKind::kBeginOrEnd;

  BlockHeader metadata(large_size, kind);

  EXPECT_EQ(metadata.BlockSize(), large_size);
  EXPECT_EQ(metadata.Kind(), kind);
}

}  // namespace blocks
}  // namespace jsmalloc

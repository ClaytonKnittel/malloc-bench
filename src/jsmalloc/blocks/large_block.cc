#include "src/jsmalloc/blocks/large_block.h"

#include <cstddef>

#include "src/jsmalloc/blocks/block.h"
#include "src/jsmalloc/blocks/free_block.h"
#include "src/jsmalloc/blocks/free_block_allocator.h"
#include "src/jsmalloc/util/assert.h"
#include "src/jsmalloc/util/math.h"

namespace jsmalloc {
namespace blocks {

class LargeBlockHelper {
  static_assert(offsetof(LargeBlock, data_) % 16 == 0);

  static size_t LargeBlockSizeForDataSize(size_t data_size) {
    return offsetof(LargeBlock, data_) + math::round_16b(data_size);
  }
};

LargeBlock::LargeBlock(size_t block_size, bool prev_block_is_free)
    : header_(block_size, BlockKind::kLarge, prev_block_is_free) {
  data_preamble_.offset = offsetof(LargeBlock, data_);
}

size_t LargeBlock::BlockSize() const {
  return header_.BlockSize();
}

size_t LargeBlock::DataSize() const {
  return header_.BlockSize() - offsetof(LargeBlock, data_);
}

void* LargeBlock::Data() {
  return static_cast<void*>(data_);
}

size_t LargeBlock::BlockSize(size_t data_size) {
  size_t block_size = offsetof(LargeBlock, data_) + math::round_16b(data_size);
  DCHECK_EQ(block_size % 16, 0);
  return block_size;
}

LargeBlock* LargeBlock::Init(FreeBlock* block) {
  return new (block)
      LargeBlock(block->BlockSize(), block->Header()->PrevBlockIsFree());
}

}  // namespace blocks
}  // namespace jsmalloc

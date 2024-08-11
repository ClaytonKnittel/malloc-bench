#include "src/jsmalloc/blocks/large_block.h"

#include <cstddef>

#include "src/jsmalloc/blocks/block.h"
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

LargeBlock::LargeBlock(size_t block_size)
    : header_(block_size, BlockKind::kLarge) {
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

LargeBlock* LargeBlock::New(Allocator& allocator, size_t data_size) {
  size_t block_size = offsetof(LargeBlock, data_) + math::round_16b(data_size);
  DCHECK_EQ(block_size % 16, 0);

  void* ptr = allocator.Allocate(block_size);
  if (ptr == nullptr) {
    return nullptr;
  }

  return new (ptr) LargeBlock(block_size);
}

}  // namespace blocks
}  // namespace jsmalloc

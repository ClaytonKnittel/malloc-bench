#include "src/jsmalloc/blocks/large_block.h"

#include <cstddef>

#include "src/jsmalloc/blocks/block.h"
#include "src/jsmalloc/blocks/free_block.h"
#include "src/jsmalloc/util/assert.h"
#include "src/jsmalloc/util/math.h"
#include "src/jsmalloc/util/twiddle.h"

namespace jsmalloc {
namespace blocks {

LargeBlock::LargeBlock(size_t block_size, bool prev_block_is_free)
    : header_(block_size, BlockKind::kLarge, prev_block_is_free) {}

size_t LargeBlock::BlockSize() const {
  return header_.BlockSize();
}

size_t LargeBlock::DataSize() const {
  return header_.BlockSize() - offsetof(LargeBlock, data_);
}

void* LargeBlock::Data() {
  return twiddle::AddPtrOffset<void>(this, data_offset_);
}

size_t LargeBlock::BlockSize(size_t data_size) {
  size_t block_size = offsetof(LargeBlock, data_) + math::round_16b(data_size);
  DCHECK_EQ(block_size % 16, 0);
  return block_size;
}

int32_t* DataPrefix(void* data_ptr) {
  return twiddle::AddPtrOffset<int32_t>(
      data_ptr, -static_cast<int32_t>(sizeof(uint32_t)));
  ;
}

LargeBlock* LargeBlock::FromDataPtr(void* ptr) {
  return twiddle::AddPtrOffset<LargeBlock>(ptr, -(*DataPrefix(ptr)));
  ;
}

LargeBlock* LargeBlock::Init(FreeBlock* block, size_t alignment) {
  auto* large_block = new (block)
      LargeBlock(block->BlockSize(), block->Header()->PrevBlockIsFree());

  void* ptr = twiddle::Align(large_block->data_, alignment);
  int32_t data_offset = twiddle::PtrValue(ptr) - twiddle::PtrValue(large_block);

  *DataPrefix(ptr) = data_offset;
  large_block->data_offset_ = data_offset;

  return large_block;
}

}  // namespace blocks
}  // namespace jsmalloc

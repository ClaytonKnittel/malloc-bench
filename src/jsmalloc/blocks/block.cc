#include "src/jsmalloc/blocks/block.h"

#include <cstdint>

#include "src/jsmalloc/util/assert.h"
#include "src/jsmalloc/util/twiddle.h"

namespace jsmalloc {
namespace blocks {

BlockHeader::BlockHeader(uint32_t size, BlockKind kind,
                         bool prev_block_is_free) {
  SetBlockSize(size);
  SetKind(kind);
  SetPrevBlockIsFree(prev_block_is_free);
}

uint32_t BlockHeader::BlockSize() const {
  DCHECK_TRUE(IsValid());
  return block_size_ << 4;
}

void BlockHeader::SetBlockSize(uint32_t size) {
  DCHECK_EQ(size % 16, 0);
  block_size_ = size >> 4;
}

BlockKind BlockHeader::Kind() const {
  DCHECK_TRUE(IsValid());
  return kind_;
}

void BlockHeader::SetKind(BlockKind kind) {
  kind_ = kind;
}

bool BlockHeader::PrevBlockIsFree() const {
  return prev_block_is_free_;
}

void BlockHeader::SetPrevBlockIsFree(bool value) {
  prev_block_is_free_ = value;
}

DataPreamble* DataPreambleFromDataPtr(void* ptr) {
  return reinterpret_cast<DataPreamble*>(ptr) - 1;
}

BlockHeader* BlockHeader::FromDataPtr(void* ptr) {
  auto* block = twiddle::AddPtrOffset<BlockHeader>(
      ptr, -DataPreambleFromDataPtr(ptr)->offset);
  DCHECK_TRUE(block->IsValid());
  return block;
}

void BlockHeader::SignalFreeToNextBlock(bool free) {
  if (Kind() == BlockKind::kEnd) {
    return;
  }
  auto* next = twiddle::AddPtrOffset<BlockHeader>(this, BlockSize());
  next->SetPrevBlockIsFree(free);
}

bool BlockHeader::IsValid() const {
#ifdef ENABLE_MAGIC_CHECKS
  if (magic_ != BlockHeader::kMagicValue) {
    std::cerr << "incorrect magic: 0x" << std::hex << magic_ << std::endl;
    return false;
  }
  return true;
#else
  return true;
#endif
}

}  // namespace blocks
}  // namespace jsmalloc

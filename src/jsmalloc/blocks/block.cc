#include "src/jsmalloc/blocks/block.h"

#include <cstdint>

#include "src/jsmalloc/util/assert.h"
#include "src/jsmalloc/util/twiddle.h"

namespace jsmalloc {
namespace blocks {
namespace {

using KindAccessor = twiddle::BitRangeAccessor<uint32_t, 0, 2>;
using PrevBlockIsFreeAccessor = twiddle::BitRangeAccessor<uint32_t, 2, 3>;
using BlockSizeAccessor = twiddle::BitRangeAccessor<uint32_t, 3, 32>;

}  // namespace

BlockHeader::BlockHeader(uint32_t size, BlockKind kind, bool prev_block_is_free)
    : data_(0) {
  SetBlockSize(size);
  SetKind(kind);
  SetPrevBlockIsFree(prev_block_is_free);
}

BlockHeader::BlockHeader(uint32_t size, BlockKind kind)
    : BlockHeader(size, kind, false) {}

uint32_t BlockHeader::BlockSize() const {
  DCHECK_TRUE(IsValid());
  return BlockSizeAccessor::Get(data_) << 4;
}

void BlockHeader::SetBlockSize(uint32_t size) {
  DCHECK_EQ(size % 16, 0);
  data_ = BlockSizeAccessor::Set(data_, size >> 4);
}

BlockKind BlockHeader::Kind() const {
  DCHECK_TRUE(IsValid());
  return static_cast<BlockKind>(KindAccessor::Get(data_));
}

void BlockHeader::SetKind(BlockKind kind) {
  data_ = KindAccessor::Set(data_, static_cast<uint32_t>(kind));
}

bool BlockHeader::PrevBlockIsFree() const {
  return static_cast<bool>(PrevBlockIsFreeAccessor::Get(data_));
}

void BlockHeader::SetPrevBlockIsFree(bool value) {
  data_ = PrevBlockIsFreeAccessor::Set(data_, static_cast<uint32_t>(value));
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

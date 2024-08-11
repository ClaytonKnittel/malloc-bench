#include "src/jsmalloc/blocks/block.h"

#include <cstdint>

#include "src/jsmalloc/util/assert.h"
#include "src/jsmalloc/util/twiddle.h"

namespace jsmalloc {
namespace blocks {
namespace {}  // namespace

BlockHeader::BlockHeader(uint32_t size, BlockKind kind) : data_(0) {
  SetBlockSize(size);
  SetKind(kind);
}

uint32_t BlockHeader::BlockSize() const {
  DCHECK_TRUE(IsValid());
  return twiddle::GetBits(data_, 2, 32) << 2;
}

void BlockHeader::SetBlockSize(uint32_t size) {
  DCHECK_EQ((size >> 2) << 2, size);
  data_ = twiddle::SetBits(data_, size >> 2, 2, 32);
}

BlockKind BlockHeader::Kind() const {
  DCHECK_TRUE(IsValid());
  return static_cast<BlockKind>(twiddle::GetBits(data_, 0, 2));
}

void BlockHeader::SetKind(BlockKind kind) {
  data_ = twiddle::SetBits(data_, static_cast<uint32_t>(kind), 0, 2);
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

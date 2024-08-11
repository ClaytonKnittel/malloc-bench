#include "src/jsmalloc/blocks/block.h"

#include <cstdint>
#include <ios>
#include <iostream>

#include "src/jsmalloc/util/assert.h"

namespace jsmalloc {
namespace blocks {
namespace {

constexpr uint32_t BitMask(uint32_t start, uint32_t end) {
  return ((1 << (end - start)) - 1) << start;
}

constexpr uint32_t SetBits(uint32_t dst, uint32_t src, uint32_t start,
                           uint32_t end) {
  DCHECK_EQ((src & BitMask(0, end - start)), src);
  dst &= ~BitMask(start, end);
  dst |= src << start;
  return dst;
}

constexpr uint32_t GetBits(uint32_t src, uint32_t start, uint32_t end) {
  src >>= start;
  src &= BitMask(0, end - start);
  return src;
}

uint32_t PtrValue(void* ptr) {
  return reinterpret_cast<uint8_t*>(ptr) - static_cast<uint8_t*>(nullptr);
}

}  // namespace

BlockHeader::BlockHeader(uint32_t size, BlockKind kind) : data_(0) {
  DCHECK_EQ(PtrValue(this) % 16, 0);

  SetBlockSize(size);
  SetKind(kind);
}

uint32_t BlockHeader::BlockSize() const {
  DCHECK_TRUE(IsValid());
  return GetBits(data_, 2, 32) << 2;
}

void BlockHeader::SetBlockSize(uint32_t size) {
  DCHECK_EQ((size >> 2) << 2, size);
  data_ = SetBits(data_, size >> 2, 2, 32);
}

BlockKind BlockHeader::Kind() const {
  DCHECK_TRUE(IsValid());
  return static_cast<BlockKind>(GetBits(data_, 0, 2));
}

void BlockHeader::SetKind(BlockKind kind) {
  data_ = SetBits(data_, static_cast<uint32_t>(kind), 0, 2);
}

DataPreamble* DataPreambleFromDataPtr(void* ptr) {
  return reinterpret_cast<DataPreamble*>(ptr) - 1;
}

BlockHeader* BlockHeader::FromDataPtr(void* ptr) {
  auto* block = reinterpret_cast<BlockHeader*>(
      reinterpret_cast<uint8_t*>(ptr) - DataPreambleFromDataPtr(ptr)->offset);
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

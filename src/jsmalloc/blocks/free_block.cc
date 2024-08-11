#include "src/jsmalloc/blocks/free_block.h"

#include <cstddef>

#include "src/jsmalloc/allocator.h"
#include "src/jsmalloc/blocks/block.h"
#include "src/jsmalloc/util/assert.h"
#include "src/jsmalloc/util/math.h"

namespace jsmalloc {
namespace blocks {
namespace {

constexpr size_t kMinFreeBlockSize = math::round_16b(sizeof(FreeBlock));

}  // namespace

FreeBlock* FreeBlock::Claim(BlockHeader* block_header) {
  DCHECK_TRUE(block_header->BlockSize() >= kMinFreeBlockSize);
  return new (block_header) FreeBlock(block_header->BlockSize());
}

FreeBlock* FreeBlock::New(Allocator& allocator, size_t size) {
  DCHECK_TRUE(size >= kMinFreeBlockSize);
  void* ptr = allocator.Allocate(size);
  if (ptr == nullptr) {
    return nullptr;
  }
  return new (ptr) FreeBlock(size);
}

bool FreeBlock::CanResizeTo(size_t new_block_size) const {
  bool exact_match = BlockSize() == new_block_size;

  bool block_has_enough_space = BlockSize() >= new_block_size;
  bool new_block_big_enough = new_block_size >= kMinFreeBlockSize;
  bool remainder_big_enough =
      (BlockSize() - new_block_size) >= kMinFreeBlockSize;

  return exact_match || (block_has_enough_space && new_block_big_enough &&
                         remainder_big_enough);
}

FreeBlock* FreeBlock::ResizeTo(size_t new_block_size) {
  DCHECK_TRUE(CanResizeTo(new_block_size));

  size_t split_block_size = BlockSize() - new_block_size;
  if (split_block_size == 0) {
    return nullptr;
  }

  new (this) FreeBlock(new_block_size);

  void* split_block_ptr = reinterpret_cast<uint8_t*>(this) + new_block_size;
  return new (split_block_ptr) FreeBlock(split_block_size);
}

size_t FreeBlock::BlockSize() const {
  return header_.BlockSize();
}

BlockHeader* FreeBlock::Header() {
  return &header_;
}

FreeBlock::FreeBlock(size_t size) : header_(size, BlockKind::kFree) {}

}  // namespace blocks
}  // namespace jsmalloc

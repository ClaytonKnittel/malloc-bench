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
  bool this_block_ok =
      BlockSize() >= new_block_size && new_block_size >= kMinFreeBlockSize;

  size_t next_block_size = BlockSize() - new_block_size;
  bool next_block_ok =
      next_block_size == 0 || next_block_size >= kMinFreeBlockSize;

  return this_block_ok && next_block_ok;
}

FreeBlock* FreeBlock::ResizeTo(size_t new_block_size) {
  DCHECK_TRUE(CanResizeTo(new_block_size));

  size_t next_block_size = BlockSize() - new_block_size;
  if (next_block_size == 0) {
    return nullptr;
  }

  new (this) FreeBlock(new_block_size);

  void* next_block_ptr = twiddle::AddPtrOffset<void*>(this, new_block_size);
  return new (next_block_ptr) FreeBlock(next_block_size);
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

#include "src/jsmalloc/blocks/free_block.h"

#include <cstddef>

#include "src/jsmalloc/allocator.h"
#include "src/jsmalloc/blocks/block.h"
#include "src/jsmalloc/blocks/sentinel_block_allocator.h"
#include "src/jsmalloc/util/assert.h"
#include "src/jsmalloc/util/math.h"

namespace jsmalloc {
namespace blocks {
namespace {

constexpr size_t kMinFreeBlockSize =
    math::round_16b(sizeof(FreeBlock) + sizeof(FreeBlockFooter));

}  // namespace

FreeBlock* FreeBlock::New(SentinelBlockHeap& heap, size_t size) {
  DCHECK_TRUE(size >= kMinFreeBlockSize);
  SentinelBlock* ptr = heap.sbrk(size);
  if (ptr == nullptr) {
    return nullptr;
  }
  return new (ptr) FreeBlock(size, ptr->Header()->PrevBlockIsFree());
}

FreeBlock* FreeBlock::MarkFree(BlockHeader* block_header) {
  DCHECK_TRUE(block_header->BlockSize() >= kMinFreeBlockSize);
  block_header->SignalFreeToNextBlock(true);
  return new (block_header)
      FreeBlock(block_header->BlockSize(), block_header->PrevBlockIsFree());
}

/** Consumes the next block. */
FreeBlock* FreeBlock::NextBlockIfFree() {
  auto* next = twiddle::AddPtrOffset<BlockHeader>(this, this->BlockSize());
  if (next->Kind() == BlockKind::kFree) {
    return reinterpret_cast<FreeBlock*>(next);
  }
  return nullptr;
}

/** Returns the previous block, if free. */
FreeBlock* FreeBlock::PrevBlockIfFree() {
  if (!Header()->PrevBlockIsFree()) {
    return nullptr;
  }
  auto* footer = reinterpret_cast<FreeBlockFooter*>(this) - 1;
  return footer->block;
}

/** Consumes the next block. */
void FreeBlock::ConsumeNextBlock() {
  auto* next = twiddle::AddPtrOffset<BlockHeader>(this, this->BlockSize());
  DCHECK_EQ(next->Kind(), BlockKind::kFree);
  new (this)
      FreeBlock(BlockSize() + next->BlockSize(), Header()->PrevBlockIsFree());
}

bool FreeBlock::CanMarkUsed(size_t new_block_size) const {
  bool this_block_ok =
      BlockSize() >= new_block_size && new_block_size >= kMinFreeBlockSize;

  size_t next_block_size = BlockSize() - new_block_size;
  bool next_block_ok =
      next_block_size == 0 || next_block_size >= kMinFreeBlockSize;

  return this_block_ok && next_block_ok;
}

FreeBlock* FreeBlock::MarkUsed(size_t new_block_size) {
  DCHECK_TRUE(CanMarkUsed(new_block_size));

  size_t next_block_size = BlockSize() - new_block_size;
  if (next_block_size == 0) {
    this->Header()->SignalFreeToNextBlock(false);
    this->Header()->SetKind(BlockKind::kLeasedFreeBlock);
    return nullptr;
  }

  new (this) FreeBlock(new_block_size, this->Header()->PrevBlockIsFree());

  void* next_block_ptr = twiddle::AddPtrOffset<void*>(this, new_block_size);
  return new (next_block_ptr)
      FreeBlock(next_block_size, /*prev_block_is_free=*/false);
}

FreeBlock* FreeBlock::MarkUsed() {
  return MarkUsed(BlockSize());
}

size_t FreeBlock::BlockSize() const {
  return header_.BlockSize();
}

BlockHeader* FreeBlock::Header() {
  return &header_;
}

FreeBlock::FreeBlock(size_t size, bool prev_block_is_free)
    : header_(size, BlockKind::kFree, prev_block_is_free) {
  auto* footer = twiddle::AddPtrOffset<FreeBlockFooter>(
      this, size - sizeof(FreeBlockFooter));
  footer->block = this;
}

}  // namespace blocks
}  // namespace jsmalloc

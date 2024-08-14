#include "src/jsmalloc/blocks/free_block_allocator.h"

#include "src/jsmalloc/allocator.h"
#include "src/jsmalloc/blocks/block.h"
#include "src/jsmalloc/blocks/free_block.h"
#include "src/jsmalloc/util/assert.h"

namespace jsmalloc {
namespace blocks {

FreeBlockAllocator::FreeBlockAllocator(Allocator& allocator)
    : allocator_(allocator){};

FreeBlock* FreeBlockAllocator::Allocate(size_t size) {
  DCHECK_EQ(size % 16, 0);

  FreeBlock* best_fit = nullptr;
  for (auto& free_block : free_blocks_) {
    if (!free_block.CanMarkUsed(size)) {
      continue;
    }
    if (best_fit == nullptr || best_fit->BlockSize() >= free_block.BlockSize()) {
      best_fit = &free_block;
    }
  }

  if (best_fit != nullptr) {
    FreeBlock& free_block = *best_fit;
    free_blocks_.remove(free_block);
    FreeBlock* remainder = free_block.MarkUsed(size);

    // Don't bother storing tiny blocks in the free list,
    // since they'll probably never be used.
    if (remainder != nullptr && remainder->BlockSize() >= 256) {
      free_blocks_.insert_back(*remainder);
    }
    free_block.Header()->SetKind(BlockKind::kLeasedFreeBlock);
    return &free_block;
  }

  FreeBlock* block = FreeBlock::New(allocator_, size);
  if (block == nullptr) {
    return nullptr;
  }
  block->Header()->SetKind(BlockKind::kLeasedFreeBlock);
  return block;
}

void FreeBlockAllocator::Free(BlockHeader* block) {
  FreeBlock* free_block = FreeBlock::MarkFree(block);

  FreeBlock* next_free_block = free_block->NextBlockIfFree();
  if (next_free_block != nullptr) {
    if (free_blocks_.contains(*next_free_block)) {
      free_blocks_.remove(*next_free_block);
    }
    free_block->ConsumeNextBlock();
  }

  FreeBlock* prev_free_block = free_block->PrevBlockIfFree();
  if (prev_free_block != nullptr) {
    if (free_blocks_.contains(*prev_free_block)) {
      free_blocks_.remove(*prev_free_block);
    }
    prev_free_block->ConsumeNextBlock();
    free_block = prev_free_block;    
  }

  free_blocks_.insert_front(*free_block);
}

}  // namespace blocks
}  // namespace jsmalloc

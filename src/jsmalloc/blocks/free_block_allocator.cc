#include "src/jsmalloc/blocks/free_block_allocator.h"

#include "src/jsmalloc/blocks/block.h"
#include "src/jsmalloc/blocks/free_block.h"
#include "src/jsmalloc/blocks/sentinel_block_allocator.h"
#include "src/jsmalloc/util/assert.h"

namespace jsmalloc {
namespace blocks {

FreeBlockAllocator::FreeBlockAllocator(SentinelBlockHeap& heap) : heap_(heap){};

FreeBlock* FreeBlockAllocator::FindBestFit(size_t size) {
  DCHECK_EQ(size % 16, 0);
  return free_blocks_.LowerBound(
      [size](const FreeBlock& block) { return block.BlockSize() >= size; });
}

FreeBlock* FreeBlockAllocator::Allocate(size_t size) {
  DCHECK_EQ(size % 16, 0);

  FreeBlock* best_fit = FindBestFit(size);
  if (best_fit != nullptr) {
    FreeBlock& free_block = *best_fit;
    free_blocks_.Remove(&free_block);
    FreeBlock* remainder = free_block.MarkUsed(size);
    // Don't bother storing small free blocks.
    // Small malloc sizes will be serviced by SmallBlockAllocator anyway.
    if (remainder != nullptr && remainder->BlockSize() > 256) {
      free_blocks_.Insert(remainder);
    }
    return &free_block;
  }

  FreeBlock* block = FreeBlock::New(heap_, size);
  if (block == nullptr) {
    return nullptr;
  }
  block->MarkUsed();
  return block;
}

void FreeBlockAllocator::Free(BlockHeader* block) {
  FreeBlock* free_block = FreeBlock::MarkFree(block);

  FreeBlock* next_free_block = free_block->NextBlockIfFree();
  if (next_free_block != nullptr) {
    free_blocks_.Remove(next_free_block);
    free_block->ConsumeNextBlock();
  }

  FreeBlock* prev_free_block = free_block->PrevBlockIfFree();
  if (prev_free_block != nullptr) {
    free_blocks_.Remove(prev_free_block);
    prev_free_block->ConsumeNextBlock();
    free_block = prev_free_block;
  }

  free_blocks_.Insert(free_block);
}

}  // namespace blocks
}  // namespace jsmalloc

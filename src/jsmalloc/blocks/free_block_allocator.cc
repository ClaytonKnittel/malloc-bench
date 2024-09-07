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
  FreeBlock* best_fit = nullptr;
  for (auto& free_block : free_blocks_) {
    if (!free_block.CanMarkUsed(size)) {
      continue;
    }
    if (best_fit == nullptr ||
        best_fit->BlockSize() >= free_block.BlockSize()) {
      best_fit = &free_block;
    }
  }
  return best_fit;
}

FreeBlock* FreeBlockAllocator::Allocate(size_t size) {
  DCHECK_EQ(size % 16, 0);

  FreeBlock* best_fit = FindBestFit(size);
  if (best_fit != nullptr) {
    FreeBlock& free_block = *best_fit;
    free_blocks_.remove(free_block);
    FreeBlock* remainder = free_block.MarkUsed(size);
    if (remainder != nullptr) {
      free_blocks_.insert_back(*remainder);
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

/**
 * Returns a pointer to some free space of exactly the given size.
 *
 * Will not request more space from system memory.
 */
FreeBlock* FreeBlockAllocator::AllocateExistingBlock(size_t min_size,
                                                     size_t max_size) {
  FreeBlock* best_fit = FindBestFit(min_size);
  if (best_fit == nullptr) {
    return nullptr;
  }
  if (best_fit->BlockSize() >= max_size) {
    return nullptr;
  }
  free_blocks_.remove(*best_fit);
  best_fit->MarkUsed();
  return best_fit;
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

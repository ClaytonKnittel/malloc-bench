#include "src/jsmalloc/blocks/free_block_allocator.h"

#include "src/jsmalloc/blocks/block.h"
#include "src/jsmalloc/blocks/free_block.h"
#include "src/jsmalloc/blocks/freelists/learned_size_free_list.h"
#include "src/jsmalloc/blocks/freelists/small_size_free_list.h"
#include "src/jsmalloc/blocks/sentinel_block_allocator.h"
#include "src/jsmalloc/util/assert.h"

#include "absl/base/prefetch.h"

namespace jsmalloc {
namespace blocks {

size_t hits = 0;
size_t total = 0;

FreeBlockAllocator::FreeBlockAllocator(SentinelBlockHeap& heap) : heap_(heap){};

FreeBlock* FreeBlockAllocator::FindBestFit(size_t size) {
  DCHECK_EQ(size % 16, 0);
  if (size <= SmallSizeFreeList::kMaxSize) {
    FreeBlock* block = small_size_free_list_.FindBestFit(size);
    if (block != nullptr) {
      return block;
    }
  } else {
    FreeBlock* block = learned_size_free_list_.FindBestFit(size);
    if (block != nullptr) {
      return block;
    }
  }
  return rbtree_free_list_.FindBestFit(size);
}

void FreeBlockAllocator::Remove(FreeBlock* block) {
  switch (block->GetStorageLocation()) {
    case FreeBlock::StorageLocation::kRbTree:
      rbtree_free_list_.Remove(block);
      break;
    case FreeBlock::StorageLocation::kSmallSizeFreeList:
      small_size_free_list_.Remove(block);
      break;
    case FreeBlock::StorageLocation::kLearnedSizeList:
      learned_size_free_list_.MaybeRemove(block);
      break;
    case FreeBlock::StorageLocation::kUntracked:
      break;
  }
}

void FreeBlockAllocator::Insert(FreeBlock* block) {
  if (block->BlockSize() <= SmallSizeFreeList::kMaxSize) {
    small_size_free_list_.Insert(block);
    return;
  }
  if (learned_size_free_list_.MaybeInsert(block)) {
    return;
  }
  rbtree_free_list_.Insert(block);
}

FreeBlock* FreeBlockAllocator::Allocate(size_t size) {
  DCHECK_EQ(size % 16, 0);

  FreeBlock* best_fit = FindBestFit(size);
  if (best_fit != nullptr) {
    Remove(best_fit);
    FreeBlock* remainder = best_fit->MarkUsed(size);
    if (remainder != nullptr) {
      Insert(remainder);
    }
    return best_fit;
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
    Remove(next_free_block);
    free_block->ConsumeNextBlock();
  }

  FreeBlock* prev_free_block = free_block->PrevBlockIfFree();
  if (prev_free_block != nullptr) {
    Remove(prev_free_block);
    prev_free_block->ConsumeNextBlock();
    free_block = prev_free_block;
  }

  Insert(free_block);
}

}  // namespace blocks
}  // namespace jsmalloc

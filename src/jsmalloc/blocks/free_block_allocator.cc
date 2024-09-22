#include "src/jsmalloc/blocks/free_block_allocator.h"

#include "src/jsmalloc/blocks/block.h"
#include "src/jsmalloc/blocks/free_block.h"
#include "src/jsmalloc/blocks/sentinel_block_allocator.h"
#include "src/jsmalloc/util/assert.h"
#include "src/jsmalloc/util/math.h"

namespace jsmalloc {
namespace blocks {

FreeBlockAllocator::FreeBlockAllocator(SentinelBlockHeap& heap) : heap_(heap) {
  empty_exact_size_lists_.SetRange(0, kExactSizeBins);
};

FreeBlock* FreeBlockAllocator::FindBestFit(size_t size) {
  DCHECK_EQ(size % 16, 0);

  if (size <= kMaxSizeForExactBins) {
    size_t idx = empty_exact_size_lists_.FindFirstUnsetBitFrom(
        math::div_ceil(size, kBytesPerExactSizeBin));
    if (idx < kExactSizeBins) {
      return exact_size_lists_[idx].front();
    }
  }

  return free_blocks_.LowerBound(
      [size](const FreeBlock& block) { return block.BlockSize() >= size; });
}

void FreeBlockAllocator::Remove(FreeBlock* block) {
  if (block->BlockSize() <= kMaxSizeForExactBins) {
    size_t idx = math::div_ceil(block->BlockSize(), kBytesPerExactSizeBin);

    FreeBlock::List::unlink(*block);
    empty_exact_size_lists_.Set(idx, exact_size_lists_[idx].empty());
  } else {
    free_blocks_.Remove(block);
  }
}

void FreeBlockAllocator::Insert(FreeBlock* block) {
  if (block->BlockSize() <= kMaxSizeForExactBins) {
    size_t idx = math::div_ceil(block->BlockSize(), kBytesPerExactSizeBin);

    exact_size_lists_[idx].insert_front(*block);
    empty_exact_size_lists_.Set(idx, false);
  } else {
    free_blocks_.Insert(block);
  }
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

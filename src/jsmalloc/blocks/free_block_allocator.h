#pragma once

#include "src/jsmalloc/blocks/block.h"
#include "src/jsmalloc/blocks/free_block.h"
#include "src/jsmalloc/blocks/sentinel_block_allocator.h"

namespace jsmalloc {
namespace blocks {

/**
 * An allocator of free blocks.
 */
class FreeBlockAllocator {
 public:
  /**
   * Returns a FreeBlock allocator that depends
   * on the provided heap-based allocator.
   */
  explicit FreeBlockAllocator(SentinelBlockHeap& heap);

  /**
   * Returns a pointer to some free space of exactly the given size.
   */
  FreeBlock* Allocate(size_t size);

  /**
   * Marks the block as free.
   */
  void Free(BlockHeader* block);

 private:
  FreeBlock* FindBestFit(size_t size);
  void Remove(FreeBlock* block);
  void Insert(FreeBlock* block);

  SentinelBlockHeap& heap_;
  FreeBlock::Tree free_blocks_;
};

}  // namespace blocks
}  // namespace jsmalloc

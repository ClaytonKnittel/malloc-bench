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
   * Returns a pointer to some free space of exactly the given size.
   *
   * Will not request more space from system memory.
   */
  FreeBlock* AllocateExistingBlock(size_t min_size, size_t max_size);

  /**
   * Marks the block as free.
   */
  void Free(BlockHeader* block);

 private:
  FreeBlock* FindBestFit(size_t size);

  SentinelBlockHeap& heap_;
  FreeBlock::List free_blocks_;
};

namespace testing {

/** A FreeBlockAllocator that can be initialized on the stack. */
// class StackFreeBlockAllocator : public FreeBlockAllocator {
//  public:
//   StackFreeBlockAllocator() : FreeBlockAllocator(stack_allocator_){};

//  private:
//   StackBasedHeap stack_allocator_;
// };

}  // namespace testing

}  // namespace blocks
}  // namespace jsmalloc

#pragma once

#include <cstddef>

#include "src/jsmalloc/allocator.h"
#include "src/jsmalloc/blocks/block.h"
#include "src/jsmalloc/blocks/sentinel_block_allocator.h"
#include "src/jsmalloc/collections/intrusive_linked_list.h"

namespace jsmalloc {
namespace blocks {

class FreeBlock;
struct FreeBlockFooter {
  FreeBlock* block;
};

/**
 * A free block.
 */
class FreeBlock {
 public:
  /** Creates a new free block. */
  static FreeBlock* New(SentinelBlockHeap& heap, size_t size);

  /** Marks the provided block as free. */
  static FreeBlock* MarkFree(BlockHeader* block);

  /**
   * Marks this block as used.
   *
   * Resizes this block and returns a newly sharded `FreeBlock`, if created.
   * Returns nullptr if no new free block was created.
   */
  FreeBlock* MarkUsed(size_t new_block_size);

  /**
   * Marks this block as used.
   */
  FreeBlock* MarkUsed();

  /**
   * Whether `MarkUsed` can be called with a resize.
   */
  bool CanMarkUsed(size_t new_block_size) const;

  /**
   * The size of this free block.
   */
  size_t BlockSize() const;

  BlockHeader* Header();

  /** Consumes the next block. */
  FreeBlock* NextBlockIfFree();

  /** Returns the previous block, if free. */
  FreeBlock* PrevBlockIfFree();

  /** Consumes the next block. */
  void ConsumeNextBlock();

  class List : public IntrusiveLinkedList<FreeBlock> {
   public:
    List() : IntrusiveLinkedList<FreeBlock>(&FreeBlock::list_node_){};
  };

 private:
  FreeBlock(size_t size, bool prev_block_is_free);

  BlockHeader header_;
  IntrusiveLinkedList<FreeBlock>::Node list_node_;
};

}  // namespace blocks
}  // namespace jsmalloc

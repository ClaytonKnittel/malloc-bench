#pragma once

#include <cstddef>

#include "src/jsmalloc/allocator.h"
#include "src/jsmalloc/blocks/block.h"
#include "src/jsmalloc/collections/intrusive_linked_list.h"

namespace jsmalloc {
namespace blocks {

/**
 * A free block.
 */
class FreeBlock {
 public:
  /** Marks the provided block as free. */
  static FreeBlock* Claim(BlockHeader* block);

  /** Creates a new free block. */
  static FreeBlock* New(Allocator& allocator, size_t size);

  /**
   * Resizes this block and returns a newly shared `FreeBlock`, if created.
   *
   * Returns nullptr if no new free block was created.
   */
  FreeBlock* ResizeTo(size_t new_block_size);

  /**
   * Whether `Resize` can be called.
   */
  bool CanResizeTo(size_t new_block_size) const;

  /**
   * The size of this free block.
   */
  size_t BlockSize() const;

  BlockHeader* Header();

  class List : public IntrusiveLinkedList<FreeBlock> {
   public:
    List() : IntrusiveLinkedList<FreeBlock>(&FreeBlock::list_node_){};
  };

 private:
  explicit FreeBlock(size_t block_size);

  BlockHeader header_;
  IntrusiveLinkedList<FreeBlock>::Node list_node_;
};

}  // namespace blocks
}  // namespace jsmalloc

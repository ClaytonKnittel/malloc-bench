#pragma once

#include <cstddef>
#include <cstring>

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/linked_list.h"

namespace ckmalloc {

class Freelist {
 public:
  // Searches the freelists for a block large enough to fit `user_size`. If none
  // is found, `nullptr` is returned.
  FreeBlock* FindFree(size_t user_size);

  // Initializes an uninitialized block to free with given size, inserting it
  // into the given freelist and returning `block` down-cast to `FreeBlock`.
  FreeBlock* InitFree(Block* block, uint64_t size);

  // This method marks this block as allocated, removes it from the free list,
  // and returns a pointer to `block` down-cast to `AllocatedBlock`, now that
  // the block has been allocated.
  AllocatedBlock* MarkAllocated(FreeBlock* block);

  // Splits this block into two blocks, allocating the first and keeping the
  // second free. The allocated block will be at least `block_size` large, and
  // the second may be null if this method decides to keep this block intact.
  // `block_size` must not be larger than the block's current size.
  std::pair<AllocatedBlock*, Block*> Split(FreeBlock* block,
                                           uint64_t block_size);

  // Marks this block as free, inserting it into the given free block list and
  // writing the footer to the end of the block and setting the "prev free" bit
  // of the next adjacent block.
  //
  // This method returns a pointer to `block` down-cast to `FreeBlock`, now that
  // the block has been freed.
  FreeBlock* MarkFree(AllocatedBlock* block);

 private:
  // Adds the block to the freelist.
  void AddBlock(FreeBlock* block);

  // Removes the block from the freelist.
  void RemoveBlock(FreeBlock* block);

  LinkedList<FreeBlock> free_blocks_;
};

}  // namespace ckmalloc

#pragma once

#include <cstddef>
#include <cstring>

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/linked_list.h"

namespace ckmalloc {

class Freelist {
  friend class FreelistTest;
  friend class MainAllocatorTest;

  friend class absl::Status ValidateLargeSlabs(
      const class std::vector<struct LargeSlabInfo>&,
      const class Freelist& freelist);

 public:
  // Searches the freelists for a block large enough to fit `user_size`. If none
  // is found, `nullptr` is returned.
  TrackedBlock* FindFree(size_t user_size);

  // Initializes an uninitialized block to free with given size, inserting it
  // into the given freelist if the size is large enough, and returning `block`
  // down-cast to `FreeBlock`.
  FreeBlock* InitFree(Block* block, uint64_t size);

  // This method marks this block as allocated, removes it from the free list,
  // and returns a pointer to `block` down-cast to `AllocatedBlock`, now that
  // the block has been allocated.
  AllocatedBlock* MarkAllocated(TrackedBlock* block);

  // Splits this block into two blocks, allocating the first and keeping the
  // second free. The allocated block will be at least `block_size` large, and
  // the second may be null if this method decides to keep this block intact.
  // `block_size` must not be larger than the block's current size.
  std::pair<AllocatedBlock*, FreeBlock*> Split(TrackedBlock* block,
                                               uint64_t block_size);

  // Marks this block as free, inserting it into the given free block list and
  // writing the footer to the end of the block and setting the "prev free" bit
  // of the next adjacent block.
  //
  // This method returns a pointer to `block` down-cast to `FreeBlock`, now that
  // the block has been freed.
  FreeBlock* MarkFree(AllocatedBlock* block);

  // Tries to resize this block to `new_size` if possible. If the resize could
  // not be done because it would cause this block to overlap with another
  // allocated block, then this returns `false` and the block is not modified.
  bool ResizeIfPossible(AllocatedBlock* block, uint64_t new_size);

  // Deletes a block in the freelist, should only be called when a large slab is
  // deallocated.
  void DeleteBlock(TrackedBlock* block);

 private:
  // Adds the block to the freelist.
  void AddBlock(TrackedBlock* block);

  // Removes the block from the freelist.
  void RemoveBlock(TrackedBlock* block);

  // Moves `block` to `new_head`, resizing it to `new_size` and returning a
  // down-cast pointer to the new free block. `new_head` must move
  // forward/backward by the difference in the block's current size and
  // `new_size`.
  FreeBlock* MoveBlockHeader(FreeBlock* block, Block* new_head,
                             uint64_t new_size);

  LinkedList<TrackedBlock> free_blocks_;
};

}  // namespace ckmalloc

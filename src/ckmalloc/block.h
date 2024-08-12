#pragma once

#include <cstddef>
#include <cstdint>

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/linked_list.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

// Blocks contain metadata regarding both free and allocated regions of memory
// from large slabs.
class Block {
  friend class AllocatedBlock;

  friend constexpr size_t HeaderOffset();

 public:
  // The number of bytes of metadata at the beginning of this block which can
  // never be allocated to the user.
  static constexpr uint64_t kMetadataOverhead = sizeof(uint64_t);

  // The number of bytes to offset the first block in a large slab so that the
  // user-allocatable regions of blocks in the slab align to the required
  // alignment.
  static constexpr uint64_t kFirstBlockInSlabOffset =
      kDefaultAlignment - kMetadataOverhead;

  static const uint64_t kMinBlockSize;

  // Returns the minimum block size such that `UserDataSize()` is >=
  // `user_size`.
  static uint64_t BlockSizeForUserSize(size_t user_size);

  // Initializes an uninitialized block to free with given size, inserting it
  // into the given freelist.
  class FreeBlock* InitFree(uint64_t size,
                            LinkedList<FreeBlock>& free_block_list);

  // Initializes an uninitialized block to allocated with given size and
  // prev_free bit set accordingly.
  class AllocatedBlock* InitAllocated(uint64_t size, bool prev_free);

  // Initializes this block to a phony header, which is placed in the last 8
  // bytes of large slabs. This is a header with size 0 which will always remain
  // "allocated", tricking blocks into not trying to coalesce with their next
  // adjacent neighbor if they are at the end of a slab.
  void InitPhonyHeader(bool prev_free);

  // Returns the size of this block including metadata.
  uint64_t Size() const;

  void SetSize(uint64_t size);

  // Returns the size of user-allocatable space in this block.
  uint64_t UserDataSize() const;

  bool Free() const;

  class FreeBlock* ToFree();

  class AllocatedBlock* ToAllocated();

  Block* NextAdjacentBlock();
  const Block* NextAdjacentBlock() const;

  Block* PrevAdjacentBlock();
  const Block* PrevAdjacentBlock() const;

 protected:
  bool PrevFree() const;

  void SetPrevFree(bool free);

  // Returns the size of the previous block, which is stored in the footer of
  // the previous block (i.e. the 8 bytes before this block's header). This
  // method may only be called if `PrevFree()` is true.
  uint64_t PrevSize() const;

  // Writes to the footer of the previous block, which holds the size of the
  // previous block.
  void SetPrevSize(uint64_t size);

  // Writes the footer of this block and the prev_free bit of the next block.
  void WriteFooterAndPrevFree();

  static constexpr uint64_t kFreeBitMask = 0x1;
  static constexpr uint64_t kPrevFreeBitMask = 0x2;

  static constexpr uint64_t kSizeMask = ~(kFreeBitMask | kPrevFreeBitMask);

  // The header contains the size of the block, whether it is free, and whether
  // the previous block is free.
  uint64_t header_;
};

class FreeBlock : public Block, public LinkedListNode {
 public:
  // You can't initialize already-initialized blocks.
  class FreeBlock* InitFree(uint64_t size,
                            LinkedList<FreeBlock>& free_block_list) = delete;
  class AllocatedBlock* InitAllocated(uint64_t size) = delete;
  void InitPhonyHeader(bool prev_free) = delete;

  // Free blocks are free by definition.
  bool Free() const = delete;

  // This method marks this block as allocated, removes it from the free list it
  // is in, and returns a pointer to `this` down-cast to `AllocatedBlock`, now
  // that the block has been allocated.
  class AllocatedBlock* MarkAllocated();

  // Splits this block into two blocks, allocating the first and keeping the
  // second free. The allocated block will be at least `block_size` large, and
  // the second may be null if this method decides to keep this block intact.
  // `block_size` must not be larger than the block's current size.
  std::pair<class AllocatedBlock*, class FreeBlock*> Split(
      uint64_t block_size, LinkedList<class FreeBlock>& free_block_list);
};

class AllocatedBlock : public Block {
  friend constexpr bool UserDataOffsetValid();

 public:
  // You can't initialize already-initialized blocks.
  FreeBlock* InitFree(uint64_t size,
                      LinkedList<FreeBlock>& free_block_list) = delete;
  class AllocatedBlock* InitAllocated(uint64_t size) = delete;
  void InitPhonyHeader(bool prev_free) = delete;

  // Allocated blocks are not free by definition.
  bool Free() const = delete;

  // Returns a pointer to the beginning of the user-allocatable region of memory
  // in this block.
  void* UserDataPtr();

  // Marks this block as free, inserting it into the given free block list and
  // writing the footer to the end of the block and setting the "prev free" bit
  // of the next adjacent block.
  //
  // This method returns a pointer to `this` down-cast to `FreeBlock`, now that
  // the block has been freed.
  class FreeBlock* MarkFree(LinkedList<FreeBlock>& free_block_list);

 private:
  // The beginning of user-allocatable space in this block.
  uint8_t data_[0];
};

constexpr uint64_t Block::kMinBlockSize =
    AlignUp<uint64_t>(sizeof(FreeBlock), kDefaultAlignment);

}  // namespace ckmalloc

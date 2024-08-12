#pragma once

#include <cstddef>
#include <cstdint>

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/linked_list.h"

namespace ckmalloc {

// Blocks contain metadata regarding both free and allocated regions of memory
// from large slabs.
class Block {
  friend class AllocatedBlock;
  friend class FreeBlock;
  friend class Freelist;

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

  static constexpr uint64_t kMinBlockSize = kDefaultAlignment;

  // The smallest size of large blocks which will be tracked in the freelist.
  // Sizes smaller than this go in small slabs, and having a heterogenous list
  // of free blocks for small size classes is extra overhead we want to avoid.
  static const size_t kMinLargeSize;

  // Returns the minimum block size such that `UserDataSize()` is >=
  // `user_size`.
  static constexpr uint64_t BlockSizeForUserSize(size_t user_size);

  // Initializes an uninitialized block to allocated with given size and
  // prev_free bit set accordingly.
  class AllocatedBlock* InitAllocated(uint64_t size, bool prev_free);

  // Initializes an untracked block with given size, which is a free block not
  // tracked in the freelist..
  class UntrackedBlock* InitUntracked(uint64_t size);

  // Initializes this block to a phony header, which is placed in the last 8
  // bytes of large slabs. This is a header with size 0 which will always remain
  // "allocated", tricking blocks into not trying to coalesce with their next
  // adjacent neighbor if they are at the end of a slab.
  void InitPhonyHeader(bool prev_free);

  // Returns the size of this block including metadata.
  uint64_t Size() const;

  // Returns the size of user-allocatable space in this block.
  uint64_t UserDataSize() const;

  bool Free() const;

  // If true, this block size is not large enough to hold large block
  // allocations, and should not be allocated or placed in the freelist.
  static constexpr bool IsUntrackedSize(uint64_t size);

  // If true, this block is not large enough to hold large block allocations,
  // and should not be allocated or placed in the freelist.
  bool IsUntrackedSize() const;

  class FreeBlock* ToFree();

  class AllocatedBlock* ToAllocated();

  class UntrackedBlock* ToUntracked();

  Block* NextAdjacentBlock();
  const Block* NextAdjacentBlock() const;

  Block* PrevAdjacentBlock();
  const Block* PrevAdjacentBlock() const;

 private:
  void SetSize(uint64_t size);

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
  class AllocatedBlock* InitAllocated(uint64_t size) = delete;
  class UntrackedBlock* InitUntracked(uint64_t size) = delete;
  void InitPhonyHeader(bool prev_free) = delete;

  // Free blocks are free by definition.
  bool Free() const = delete;
};

class AllocatedBlock : public Block {
  friend constexpr size_t UserDataOffset();

 public:
  // You can't initialize already-initialized blocks.
  class AllocatedBlock* InitAllocated(uint64_t size) = delete;
  class UntrackedBlock* InitUntracked(uint64_t size) = delete;
  void InitPhonyHeader(bool prev_free) = delete;

  // Allocated blocks are not free by definition.
  bool Free() const = delete;

  // Returns a pointer to the beginning of the user-allocatable region of memory
  // in this block.
  void* UserDataPtr();

  // Given a user data pointer, returns the allocated block containing this
  // pointer.
  static AllocatedBlock* FromUserDataPtr(void* ptr);

 private:
  // The beginning of user-allocatable space in this block.
  uint8_t data_[0];
};

// Untracked blocks are free blocks that aren't in the freelist to avoid having
// heterogenous freelists for small size classes.
class UntrackedBlock : public Block {
 public:
  // You can't initialize already-initialized blocks.
  class AllocatedBlock* InitAllocated(uint64_t size) = delete;
  class UntrackedBlock* InitUntracked(uint64_t size) = delete;
  void InitPhonyHeader(bool prev_free) = delete;

  // Untracked blocks are free by definition.
  bool Free() const = delete;
};

/* static */
constexpr uint64_t Block::BlockSizeForUserSize(size_t user_size) {
  // TODO revert
  // return AlignUp(user_size + kMetadataOverhead, kDefaultAlignment);
  return std::max(AlignUp(user_size + kMetadataOverhead, kDefaultAlignment),
                  kMinBlockSize + 16);
}

/* static */
constexpr bool Block::IsUntrackedSize(uint64_t size) {
  return size < kMinLargeSize;
}

constexpr size_t Block::kMinLargeSize = BlockSizeForUserSize(kMaxSmallSize + 1);

}  // namespace ckmalloc

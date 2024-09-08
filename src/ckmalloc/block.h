#pragma once

#include <cstddef>
#include <cstdint>

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/linked_list.h"
#include "src/ckmalloc/red_black_tree.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

// Blocks contain metadata regarding both free and allocated regions of memory
// from large slabs.
class Block {
  friend class AllocatedBlock;
  friend class BlockTest;
  friend class ExactSizeBlock;
  friend class Freelist;
  friend class FreelistTest;
  friend class LargeAllocatorFixture;
  friend class TreeBlock;

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const Block& block);

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
  // Sizes smaller than this go in small slabs, as having a heterogenous list of
  // free blocks for small size classes is extra overhead we want to avoid.
  static const size_t kMinLargeSize;

  // The largest sized blocks which are tracked in single-size lists. All blocks
  // larger than this are stored in a red-black tree ordered by size.
  static constexpr uint64_t kMaxExactSizeBlock = 4096;

  // Returns the maximum user size that fits in a block of size `block_size`.
  static constexpr size_t UserSizeForBlockSize(uint64_t block_size);

  // Returns the minimum block size such that `UserDataSize()` is >=
  // `user_size`.
  static constexpr uint64_t BlockSizeForUserSize(size_t user_size);

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

  // Returns the size of user-allocatable space in this block.
  uint64_t UserDataSize() const;

  bool Free() const;

  // If true, this block size is not large enough to hold large block
  // allocations, and should not be allocated or placed in the freelist.
  static constexpr bool IsUntrackedSize(uint64_t size);

  // If true, this block is not large enough to hold large block allocations,
  // and should not be allocated or placed in the freelist.
  bool IsUntracked() const;

  // If true, this block is in the exact-size bins. Otherwise it is in the free
  // block rb tree.
  bool IsExactSize() const;

  class AllocatedBlock* ToAllocated();
  const class AllocatedBlock* ToAllocated() const;

  class FreeBlock* ToFree();
  const class FreeBlock* ToFree() const;

  class TrackedBlock* ToTracked();
  const class TrackedBlock* ToTracked() const;

  class ExactSizeBlock* ToExactSize();
  const class ExactSizeBlock* ToExactSize() const;

  class TreeBlock* ToTree();
  const class TreeBlock* ToTree() const;

  class UntrackedBlock* ToUntracked();
  const class UntrackedBlock* ToUntracked() const;

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

class FreeBlock : public Block {
 public:
  // You can't initialize already-initialized blocks.
  class AllocatedBlock* InitAllocated(uint64_t size) = delete;
  void InitPhonyHeader(bool prev_free) = delete;

  // Free blocks are free by definition.
  bool Free() const = delete;
};

// Tracked blocks are large blocks large enough to be tracked in the freelist.
class TrackedBlock : public FreeBlock {
 public:
  bool IsUntracked() const = delete;
};

// Exact size blocks are free blocks that are in a list of blocks of the same
// size.
class ExactSizeBlock : public TrackedBlock, public LinkedListNode {
 public:
  bool IsExactSize() const = delete;
};

// Tree blocks are free blocks that are in the tree of large blocks ordered by
// size.
class TreeBlock : public TrackedBlock, public RbNode {
 public:
  bool IsExactSize() const = delete;

  bool operator<(const TreeBlock& block) const {
    return Size() < block.Size();
  }
};

// Untracked blocks are free blocks that aren't in the freelist to avoid having
// heterogenous freelists for small size classes. This means they are not
// allocatable until they merge with adjacent free blocks and become large
// enough to be tracked.
class UntrackedBlock : public FreeBlock {
 public:
  // Untracked blocks are untracked by definition.
  bool IsUntracked() const = delete;
  bool IsExactSize() const = delete;
};

/* static */
constexpr size_t Block::UserSizeForBlockSize(uint64_t block_size) {
  CK_ASSERT_TRUE(IsAligned(block_size, kDefaultAlignment));
  return block_size - kMetadataOverhead;
}

/* static */
constexpr uint64_t Block::BlockSizeForUserSize(size_t user_size) {
  return AlignUp<size_t>(user_size + kMetadataOverhead, kDefaultAlignment);
}

/* static */
constexpr bool Block::IsUntrackedSize(uint64_t size) {
  return size < kMinLargeSize;
}

constexpr size_t Block::kMinLargeSize = BlockSizeForUserSize(kMaxSmallSize + 1);

}  // namespace ckmalloc

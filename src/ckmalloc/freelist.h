#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>

#include "util/bit_set.h"

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/common.h"
#include "src/ckmalloc/linked_list.h"
#include "src/ckmalloc/red_black_tree.h"

namespace ckmalloc {

// The freelist tracks free large blocks, i.e. blocks which hold regions of
// memory larger than what fit into small slabs.
class Freelist {
  friend class LargeAllocatorFixture;

 public:
  // Searches the freelists for a block at least as large as `block_size`. If
  // none is found, `nullptr` is returned.
  TrackedBlock* FindFree(uint64_t block_size);

  // Like FindFree, but returns a block that can fit an aligned allocation of
  // size `block_size`.
  TrackedBlock* FindFreeAligned(uint64_t block_size, uint64_t alignment);

  // Searches the freelists for a block at least as large as `block_size`, but
  // only checks one potentially non-empty exact-size bin. Does not check the
  // red-black tree for blocks.
  TrackedBlock* FindFreeLazy(uint64_t block_size);

  // Like FindFreeLazy, but returns a block that can fit an aligned allocation
  // of size `block_size`.
  TrackedBlock* FindFreeLazyAligned(uint64_t block_size, uint64_t alignment);

  // Initializes an uninitialized block to free with given size, inserting it
  // into the given freelist if the size is large enough, and returning `block`
  // down-cast to `FreeBlock`.
  FreeBlock* InitFree(Block* block, uint64_t size);

  // Splits this block into two blocks, allocating the first and keeping the
  // second free. The allocated block will be at least `block_size` large, and
  // the second may be null if this method decides to keep this block intact.
  // `block_size` must not be larger than the block's current size.
  std::pair<AllocatedBlock*, FreeBlock*> Split(TrackedBlock* block,
                                               uint64_t block_size);

  // Splits this block into up to 3 blocks, such that the middle block is
  // aligned to `alignment`.
  std::tuple<FreeBlock*, AllocatedBlock*, FreeBlock*> SplitAligned(
      TrackedBlock* block, uint64_t block_size, size_t alignment);

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
  static size_t ExactSizeIdx(uint64_t block_size);

  // This method marks this block as allocated, removes it from the free list,
  // and returns a pointer to `block` down-cast to `AllocatedBlock`, now that
  // the block has been allocated.
  AllocatedBlock* MarkAllocated(
      TrackedBlock* block, std::optional<uint64_t> new_size = std::nullopt);

  // Adds the block to the freelist.
  void AddBlock(TrackedBlock* block);

  // Removes the block from the freelist.
  void RemoveBlock(TrackedBlock* block);

  static constexpr size_t kNumExactSizeBins =
      (Block::kMaxExactSizeBlock - Block::kMinBlockSize) / kDefaultAlignment +
      1;

  // The skip list is a bit-set of potentially non-empty exact-size bins. When
  // new blocks are added to an exact-size bin, they set the corresponding bit
  // in the exact-bin skiplist, but this bit is only zeroed out when searching
  // the freelist and the bin is found to be empty.
  util::BitSet<kNumExactSizeBins> exact_bin_skiplist_;
  // The exact-size bins are a bunch of doubly-linked lists of blocks of all the
  // same size, ranging from the smallest allowed large block size to
  // `kMaxExactSizeBlock`.
  LinkedList<ExactSizeBlock> exact_size_bins_[kNumExactSizeBins];

  // The large blocks tree is a tree of blocks too large to go in the exact-size
  // bins sorted by size.
  RbTree<TreeBlock> large_blocks_tree_;
};

}  // namespace ckmalloc

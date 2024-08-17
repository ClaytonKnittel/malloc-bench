#pragma once

#include <cstddef>
#include <cstring>

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/common.h"
#include "src/ckmalloc/freelist.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_manager.h"
#include "src/ckmalloc/slab_map.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
class LargeAllocatorImpl {
  friend class MainAllocatorFixture;

 public:
  LargeAllocatorImpl(SlabMap* slab_map, SlabManager* slab_manager)
      : slab_map_(slab_map), slab_manager_(slab_manager) {}

  // Performs allocation for a large-sized allocation (i.e.
  // !IsSmallSize(user_size)).
  AllocatedBlock* AllocLarge(size_t user_size);

  // Performs reallocation for an allocation in a large slab. `user_size` must
  // be a large size.
  AllocatedBlock* ReallocLarge(LargeSlab* slab, AllocatedBlock* block,
                               size_t user_size);

  // Frees an allocation in a large slab.
  void FreeLarge(LargeSlab* slab, AllocatedBlock* block);

 private:
  // Releases an empty large slab back to the slab manager.
  void ReleaseLargeSlab(LargeSlab* slab);

  // Tries to find a free block large enough for `user_size`, and if one is
  // found, returns the `AllocatedBlock` large enough to serve this request.
  AllocatedBlock* MakeBlockFromFreelist(size_t user_size);

  // Allocates a new large slab large enough for `user_size`, and returns a
  // pointer to the newly created `AllocatedBlock` that is large enough for
  // `user_size`.
  AllocatedBlock* AllocLargeSlabAndMakeBlock(size_t user_size);

  SlabMap* const slab_map_;

  SlabManager* const slab_manager_;

  Freelist freelist_;
};

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
AllocatedBlock* LargeAllocatorImpl<SlabMap, SlabManager>::AllocLarge(
    size_t user_size) {
  AllocatedBlock* block = MakeBlockFromFreelist(user_size);

  // If allocating from the freelist fails, we need to request another slab of
  // memory.
  if (block == nullptr) {
    block = AllocLargeSlabAndMakeBlock(user_size);
  }

  return block;
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
AllocatedBlock* LargeAllocatorImpl<SlabMap, SlabManager>::ReallocLarge(
    LargeSlab* slab, AllocatedBlock* block, size_t user_size) {
  CK_ASSERT_GT(user_size, kMaxSmallSize);

  uint64_t block_size = block->Size();
  uint64_t new_block_size = Block::BlockSizeForUserSize(user_size);

  // If we can resize the block in-place, then we don't need to copy any data
  // and can return the same pointer back to the user.
  if (freelist_.ResizeIfPossible(block, new_block_size)) {
    slab->AddAllocation(new_block_size);
    slab->RemoveAllocation(block_size);
    return block;
  }

  // Otherwise, if resizing in-place didn't work, then we have to allocate a new
  // block and copy the contents of this one over to the new one.
  AllocatedBlock* new_block = AllocLarge(user_size);
  if (new_block != nullptr) {
    std::memcpy(new_block->UserDataPtr(), block->UserDataPtr(),
                std::min<size_t>(user_size, block->UserDataSize()));
    FreeLarge(slab, block);
  }
  return new_block;
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
void LargeAllocatorImpl<SlabMap, SlabManager>::FreeLarge(
    LargeSlab* slab, AllocatedBlock* block) {
  slab->RemoveAllocation(block->Size());
  freelist_.MarkFree(block);

  if (slab->AllocatedBytes() == 0) {
    ReleaseLargeSlab(slab);
  }
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
void LargeAllocatorImpl<SlabMap, SlabManager>::ReleaseLargeSlab(
    LargeSlab* slab) {
  CK_ASSERT_EQ(slab->AllocatedBytes(), 0);
  Block* only_block = slab_manager_->FirstBlockInLargeSlab(slab);
  CK_ASSERT_EQ(only_block->Size(), slab->MaxBlockSize());
  CK_ASSERT_TRUE(only_block->Free());
  CK_ASSERT_TRUE(!only_block->IsUntracked());

  freelist_.DeleteBlock(only_block->ToTracked());
  slab_manager_->Free(slab);
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
AllocatedBlock* LargeAllocatorImpl<SlabMap, SlabManager>::MakeBlockFromFreelist(
    size_t user_size) {
  TrackedBlock* free_block = freelist_.FindFree(user_size);
  if (free_block == nullptr) {
    return nullptr;
  }

  LargeSlab* slab =
      slab_map_->FindSlab(slab_manager_->PageIdFromPtr(free_block))->ToLarge();

  auto [allocated_block, remainder_block] =
      freelist_.Split(free_block, Block::BlockSizeForUserSize(user_size));

  slab->AddAllocation(allocated_block->Size());
  return allocated_block;
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
AllocatedBlock*
LargeAllocatorImpl<SlabMap, SlabManager>::AllocLargeSlabAndMakeBlock(
    size_t user_size) {
  uint32_t n_pages = LargeSlab::NPagesForBlock(user_size);
  auto result = slab_manager_->template Alloc<LargeSlab>(n_pages);
  if (result == std::nullopt) {
    return nullptr;
  }
  LargeSlab* slab = result->second;

  uint64_t block_size = Block::BlockSizeForUserSize(user_size);
  CK_ASSERT_LE(block_size, slab->MaxBlockSize());

  uint64_t remainder_size = slab->MaxBlockSize() - block_size;
  CK_ASSERT_TRUE(IsAligned(remainder_size, kDefaultAlignment));

  AllocatedBlock* block =
      slab_manager_->FirstBlockInLargeSlab(slab)->InitAllocated(block_size,
                                                                false);
  slab->AddAllocation(block_size);

  // Write a phony header for an allocated block of size 0 at the end of the
  // slab, which will trick the last block in the slab into never trying to
  // coalesce with its next adjacent neighbor.
  Block* slab_end_header = block->NextAdjacentBlock();

  if (remainder_size != 0) {
    Block* next_adjacent = block->NextAdjacentBlock();
    freelist_.InitFree(next_adjacent, remainder_size);

    slab_end_header = next_adjacent->NextAdjacentBlock();
    slab_end_header->InitPhonyHeader(/*prev_free=*/true);
  } else {
    slab_end_header->InitPhonyHeader(/*prev_free=*/false);
  }

  return block;
}

using LargeAllocator = LargeAllocatorImpl<SlabMap, SlabManager>;

}  // namespace ckmalloc

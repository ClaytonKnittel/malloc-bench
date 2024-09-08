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
  friend class TestMainAllocator;

 public:
  LargeAllocatorImpl(SlabMap* slab_map, SlabManager* slab_manager)
      : slab_map_(slab_map), slab_manager_(slab_manager) {}

  // Performs allocation for a large-sized allocation (i.e.
  // !IsSmallSize(user_size)).
  void* AllocLarge(size_t user_size);

  // Performs reallocation for an allocation in a large slab. `user_size` must
  // be a large size.
  void* ReallocLarge(LargeSlab* slab, void* ptr, size_t user_size);

  // Frees an allocation in a large slab.
  void FreeLarge(LargeSlab* slab, void* ptr);

 private:
  // Releases an empty blocked slab back to the slab manager.
  void ReleaseBlockedSlab(BlockedSlab* slab);

  // Tries to find a free block large enough for `user_size`, and if one is
  // found, returns the `AllocatedBlock` large enough to serve this request.
  AllocatedBlock* MakeBlockFromFreelist(size_t user_size);

  // Allocates a nearly or exactly page-multiple sized allocation in its own
  // slab.
  void* AllocPageMultipleSlabAndMakeBlock(size_t user_size);

  // Allocates a new large slab large enough for `user_size`, and returns a
  // pointer to the newly created `AllocatedBlock` that is large enough for
  // `user_size`.
  AllocatedBlock* AllocBlockedSlabAndMakeBlock(size_t user_size);

  // Allocates a single-alloc slab, returning a pointer to the single allocation
  // within that slab.
  void* AllocSingleAllocSlab(size_t user_size);

  // Tries resizing a single-alloc slab. This will only succeed if `new_size` is
  // suitable for single-alloc slabs, and the new single-alloc slab is <= the
  // current, or > and there are enough next-adjacent free slabs to extend this
  // slab.
  bool ResizeSingleAllocIfPossible(SingleAllocSlab* slab, size_t new_size);

  SlabMap* const slab_map_;

  SlabManager* const slab_manager_;

  Freelist freelist_;
};

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
void* LargeAllocatorImpl<SlabMap, SlabManager>::AllocLarge(size_t user_size) {
  AllocatedBlock* block = MakeBlockFromFreelist(user_size);
  if (block != nullptr) {
    return block->UserDataPtr();
  }

  // If allocating from the freelist fails, we need to request another slab of
  // memory.
  if (SingleAllocSlab::SizeSuitableForSingleAlloc(user_size)) {
    return AllocSingleAllocSlab(user_size);
  }

  block = AllocBlockedSlabAndMakeBlock(user_size);
  return block != nullptr ? block->UserDataPtr() : nullptr;
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
void* LargeAllocatorImpl<SlabMap, SlabManager>::ReallocLarge(LargeSlab* slab,
                                                             void* ptr,
                                                             size_t user_size) {
  CK_ASSERT_GT(user_size, kMaxSmallSize);

  uint64_t orig_user_size;
  if (slab->Type() == SlabType::kBlocked) {
    BlockedSlab* blocked_slab = slab->ToBlocked();
    AllocatedBlock* block = AllocatedBlock::FromUserDataPtr(ptr);
    uint64_t block_size = block->Size();
    uint64_t new_block_size = Block::BlockSizeForUserSize(user_size);

    // If we can resize the block in-place, then we don't need to copy any data
    // and can return the same pointer back to the user.
    if (freelist_.ResizeIfPossible(block, new_block_size)) {
      blocked_slab->AddAllocation(new_block_size);
      blocked_slab->RemoveAllocation(block_size);
      return ptr;
    }

    orig_user_size = block->UserDataSize();
  } else {
    CK_ASSERT_EQ(slab->Type(), SlabType::kSingleAlloc);
    SingleAllocSlab* single_slab = slab->ToSingleAlloc();

    if (ResizeSingleAllocIfPossible(single_slab, user_size)) {
      return ptr;
    }

    orig_user_size = single_slab->Pages() * kPageSize;
  }

  // Otherwise, if resizing in-place didn't work, then we have to allocate a new
  // block and copy the contents of this one over to the new one.
  void* new_ptr = AllocLarge(user_size);
  if (new_ptr != nullptr) {
    std::memcpy(new_ptr, ptr, std::min<size_t>(user_size, orig_user_size));
    FreeLarge(slab, ptr);
  }
  return new_ptr;
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
void LargeAllocatorImpl<SlabMap, SlabManager>::FreeLarge(LargeSlab* slab,
                                                         void* ptr) {
  if (slab->Type() == SlabType::kBlocked) {
    BlockedSlab* blocked_slab = slab->ToBlocked();
    AllocatedBlock* block = AllocatedBlock::FromUserDataPtr(ptr);
    blocked_slab->RemoveAllocation(block->Size());
    freelist_.MarkFree(block);

    if (blocked_slab->AllocatedBytes() == 0) {
      ReleaseBlockedSlab(blocked_slab);
    }
  } else {
    CK_ASSERT_EQ(slab->Type(), SlabType::kSingleAlloc);
    slab_manager_->Free(slab);
  }
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
void LargeAllocatorImpl<SlabMap, SlabManager>::ReleaseBlockedSlab(
    BlockedSlab* slab) {
  BlockedSlab* blocked_slab = slab->ToBlocked();
  CK_ASSERT_EQ(blocked_slab->AllocatedBytes(), 0);

  Block* only_block = slab_manager_->FirstBlockInBlockedSlab(blocked_slab);
  CK_ASSERT_EQ(only_block->Size(), blocked_slab->MaxBlockSize());
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

  BlockedSlab* slab =
      slab_map_->FindSlab(slab_manager_->PageIdFromPtr(free_block))
          ->ToBlocked();

  auto [allocated_block, remainder_block] =
      freelist_.Split(free_block, Block::BlockSizeForUserSize(user_size));

  slab->AddAllocation(allocated_block->Size());
  return allocated_block;
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
AllocatedBlock*
LargeAllocatorImpl<SlabMap, SlabManager>::AllocBlockedSlabAndMakeBlock(
    size_t user_size) {
  uint32_t n_pages = BlockedSlab::NPagesForBlock(user_size);
  auto result = slab_manager_->template Alloc<BlockedSlab>(n_pages);
  if (result == std::nullopt) {
    return nullptr;
  }
  BlockedSlab* slab = result->second;

  uint64_t block_size = Block::BlockSizeForUserSize(user_size);
  CK_ASSERT_LE(block_size, slab->MaxBlockSize());

  uint64_t remainder_size = slab->MaxBlockSize() - block_size;
  CK_ASSERT_TRUE(IsAligned(remainder_size, kDefaultAlignment));

  AllocatedBlock* block =
      slab_manager_->FirstBlockInBlockedSlab(slab)->InitAllocated(
          block_size, /*prev_free=*/false);
  slab->AddAllocation(block_size);

  // Write a phony header for an allocated block of size 0 at the end of the
  // slab, which will trick the last block in the slab into never trying to
  // coalesce with its next adjacent neighbor.
  Block* slab_end_header = block->NextAdjacentBlock();

  if (remainder_size != 0) {
    Block* next_adjacent = slab_end_header;
    freelist_.InitFree(next_adjacent, remainder_size);

    slab_end_header = next_adjacent->NextAdjacentBlock();
    slab_end_header->InitPhonyHeader(/*prev_free=*/true);
  } else {
    slab_end_header->InitPhonyHeader(/*prev_free=*/false);
  }

  return block;
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
void* LargeAllocatorImpl<SlabMap, SlabManager>::AllocSingleAllocSlab(
    size_t user_size) {
  CK_ASSERT_TRUE(SingleAllocSlab::SizeSuitableForSingleAlloc(user_size));
  uint32_t n_pages = SingleAllocSlab::NPagesForAlloc(user_size);
  auto result = slab_manager_->template Alloc<SingleAllocSlab>(n_pages);
  if (result == std::nullopt) {
    return nullptr;
  }

  return slab_manager_->PageStartFromId(result->first);
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
bool LargeAllocatorImpl<SlabMap, SlabManager>::ResizeSingleAllocIfPossible(
    SingleAllocSlab* slab, size_t new_size) {
  // TODO
  return false;
}

using LargeAllocator = LargeAllocatorImpl<SlabMap, SlabManager>;

}  // namespace ckmalloc

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <tuple>

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

  uint64_t block_size;
  if (slab->Type() == SlabType::kBlocked) {
    AllocatedBlock* block = AllocatedBlock::FromUserDataPtr(ptr);
    uint64_t new_block_size = Block::BlockSizeForUserSize(user_size);

    // If we can resize the block in-place, then we don't need to copy any data
    // and can return the same pointer back to the user.
    if (freelist_.ResizeIfPossible(block, new_block_size)) {
      return ptr;
    }

    block_size = block->UserDataSize();
  } else {
    CK_ASSERT_EQ(slab->Type(), SlabType::kSingleAlloc);
    SingleAllocSlab* single_slab = slab->ToSingleAlloc();

    if (ResizeSingleAllocIfPossible(single_slab, user_size)) {
      return ptr;
    }

    block_size = single_slab->Pages() * kPageSize;
  }

  // Otherwise, if resizing in-place didn't work, then we have to allocate a new
  // block and copy the contents of this one over to the new one.
  void* new_ptr = AllocLarge(user_size);
  if (new_ptr != nullptr) {
    std::memcpy(new_ptr, ptr, std::min<size_t>(user_size, block_size));
    FreeLarge(slab, ptr);
  }
  return new_ptr;
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
void LargeAllocatorImpl<SlabMap, SlabManager>::FreeLarge(LargeSlab* slab,
                                                         void* ptr) {
  if (slab->Type() == SlabType::kSingleAlloc) {
    slab_manager_->Free(slab);
    return;
  }

  CK_ASSERT_EQ(slab->Type(), SlabType::kBlocked);
  BlockedSlab* blocked_slab = slab->ToBlocked();
  AllocatedBlock* block = AllocatedBlock::FromUserDataPtr(ptr);
  FreeBlock* free_block = freelist_.MarkFree(block);

  if (blocked_slab->SpansWholeSlab(free_block)) {
    ReleaseBlockedSlab(blocked_slab);
    return;
  }

  Block* next_adjacent = free_block->NextAdjacentBlock();
  const size_t slab_start =
      reinterpret_cast<size_t>(
          slab_manager_->PageStartFromId(slab->StartId())) /
      kPageSize;

  const uint64_t block_start = reinterpret_cast<uint64_t>(free_block);
  // This is the starting page of the region of the slab that is safe to free.
  size_t first_empty_page = block_start / kPageSize + 1;
  // This is the starting page of the region of the slab we have to keep intact,
  // as it contains allocated memory.
  size_t first_intact_page =
      reinterpret_cast<size_t>(next_adjacent) / kPageSize;

  if (next_adjacent->IsPhonyHeader()) {
    // If this is the last block, we don't need to keep the phony header intact.
    first_intact_page++;
  } else if (free_block ==
             slab_manager_->FirstBlockInBlockedSlab(blocked_slab)) {
    // If this is the first block in the slab, we can free starting from the
    // beginning.
    first_empty_page = slab_start;
  }

  // If the two ends of this newly freed block straddle at least a page, free
  // the page in the middle and split the large slab.
  if (first_empty_page < first_intact_page) {
    // If this block is tracked, we need to remove it from the freelist.
    if (!free_block->IsUntracked()) {
      freelist_.DeleteBlock(free_block->ToTracked());
    }

    auto result = slab_manager_->Carve(blocked_slab,
                                       /*from=*/first_empty_page - slab_start,
                                       /*to=*/first_intact_page - slab_start);
    if (!result.has_value()) {
      // If carving the slab failed, we can gracefully terminate this operation,
      // since no state has been modified since freeing the block.
      return;
    }

    // If the carve succeeded, we need to cut the block short on both ends.
    BlockedSlab* left_slab = std::get<0>(result.value());
    BlockedSlab* right_slab = std::get<2>(result.value());

    if (left_slab != nullptr) {
      uint64_t new_size =
          kPageSize - (block_start % kPageSize) - Block::kMetadataOverhead;
      freelist_.TruncateBlock(free_block, new_size);
    }

    // If the carve left a blocked slab after the free slab in the middle, we
    // need to fix it's start.
    if (right_slab != nullptr) {
      uint64_t start_size =
          reinterpret_cast<uint64_t>(next_adjacent) % kPageSize -
          Block::kMetadataOverhead;
      // If this block previously went exactly to the beginning of a page, we
      // just need to set the prev-free bit of the next adjacent block.
      if (start_size == 0) {
        next_adjacent->SetPrevFree(false);
      } else {
        // Otherwise, we can initialize a new free block here.
        CK_ASSERT_GE(start_size, Block::kMinBlockSize);
        freelist_.InitFree(PtrSub<Block>(next_adjacent, start_size),
                           start_size);
      }
    }
  }
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
void LargeAllocatorImpl<SlabMap, SlabManager>::ReleaseBlockedSlab(
    BlockedSlab* slab) {
  BlockedSlab* blocked_slab = slab->ToBlocked();

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

  auto [allocated_block, remainder_block] =
      freelist_.Split(free_block, Block::BlockSizeForUserSize(user_size));
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
  if (!SingleAllocSlab::SizeSuitableForSingleAlloc(new_size)) {
    return false;
  }

  uint32_t n_pages = SingleAllocSlab::NPagesForAlloc(new_size);
  return slab_manager_->Resize(slab, n_pages);
}

using LargeAllocator = LargeAllocatorImpl<SlabMap, SlabManager>;

}  // namespace ckmalloc

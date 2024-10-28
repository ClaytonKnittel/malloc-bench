#pragma once

#include <cstddef>
#include <cstdint>
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
  friend class LargeAllocatorFixture;
  friend class GlobalState;
  friend class TestMainAllocator;

 public:
  LargeAllocatorImpl(SlabMap* slab_map, SlabManager* slab_manager,
                     Freelist* freelist)
      : slab_map_(slab_map), slab_manager_(slab_manager), freelist_(freelist) {}

  // Performs allocation for a large-sized allocation (i.e.
  // !IsSmallSize(user_size)).
  Void* AllocLarge(size_t user_size,
                   std::optional<size_t> alignment = std::nullopt);

  // Performs reallocation for an allocation in a large slab. `user_size` must
  // be a large size.
  Void* ReallocLarge(LargeSlab* slab, Void* ptr, size_t user_size);

  // Frees an allocation in a large slab.
  void FreeLarge(LargeSlab* slab, Void* ptr);

 private:
  // Releases an empty blocked slab back to the slab manager.
  void ReleaseBlockedSlab(BlockedSlab* slab);

  // Tries to find a free block large enough for `user_size`, and if one is
  // found, returns the `AllocatedBlock` large enough to serve this request.
  AllocatedBlock* MakeBlockFromFreelist(uint64_t block_size,
                                        std::optional<size_t> alignment);

  // Allocates a new large slab large enough for `user_size`, and returns a
  // pointer to the newly created `AllocatedBlock` that is large enough for
  // `user_size`.
  AllocatedBlock* AllocBlockedSlabAndMakeBlock(uint64_t block_size,
                                               std::optional<size_t> alignment);

  // Allocates a single-alloc slab, returning a pointer to the single allocation
  // within that slab.
  Void* AllocSingleAllocSlab(size_t user_size, std::optional<size_t> alignment);

  // Tries resizing a single-alloc slab. This will only succeed if `new_size` is
  // suitable for single-alloc slabs, and the new single-alloc slab is <= the
  // current, or > and there are enough next-adjacent free slabs to extend this
  // slab.
  bool ResizeSingleAllocIfPossible(SingleAllocSlab* slab, size_t new_size);

  SlabMap* const slab_map_;

  SlabManager* const slab_manager_;

  Freelist* const freelist_;
};

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
Void* LargeAllocatorImpl<SlabMap, SlabManager>::AllocLarge(
    size_t user_size, std::optional<size_t> alignment) {
  CK_ASSERT_LT(user_size, kMinMmapSize);
  uint64_t block_size = Block::BlockSizeForUserSize(user_size);
  AllocatedBlock* block = MakeBlockFromFreelist(block_size, alignment);
  if (block != nullptr) {
    return block->UserDataPtr();
  }

  // If allocating from the freelist fails, we need to request another slab of
  // memory.
  // TODO: do this immediately for large allocs which don't have an exact fit in
  // the freelist.
  if (SingleAllocSlab::SizeSuitableForSingleAlloc(user_size) ||
      alignment.value_or(0) >= kPageSize) {
    return AllocSingleAllocSlab(user_size, alignment);
  }

  block = AllocBlockedSlabAndMakeBlock(block_size, alignment);
  return block != nullptr ? block->UserDataPtr() : nullptr;
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
Void* LargeAllocatorImpl<SlabMap, SlabManager>::ReallocLarge(LargeSlab* slab,
                                                             Void* ptr,
                                                             size_t user_size) {
  CK_ASSERT_LT(user_size, kMinMmapSize);

  uint64_t orig_user_size;
  if (slab->Type() == SlabType::kBlocked) {
    BlockedSlab* blocked_slab = slab->ToBlocked();
    AllocatedBlock* block = AllocatedBlock::FromUserDataPtr(ptr);
    uint64_t block_size = block->Size();
    uint64_t new_block_size = Block::BlockSizeForUserSize(user_size);

    // If we can resize the block in-place, then we don't need to copy any data
    // and can return the same pointer back to the user.
    if (freelist_->ResizeIfPossible(block, new_block_size)) {
      blocked_slab->AddAllocation(block->Size());
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
  Void* new_ptr = AllocLarge(user_size);
  if (new_ptr != nullptr) {
    std::memcpy(new_ptr, ptr, std::min<size_t>(user_size, orig_user_size));
    FreeLarge(slab, ptr);
  }
  return new_ptr;
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
void LargeAllocatorImpl<SlabMap, SlabManager>::FreeLarge(LargeSlab* slab,
                                                         Void* ptr) {
  if (slab->Type() == SlabType::kBlocked) {
    BlockedSlab* blocked_slab = slab->ToBlocked();
    AllocatedBlock* block = AllocatedBlock::FromUserDataPtr(ptr);
    blocked_slab->RemoveAllocation(block->Size());
    freelist_->MarkFree(block);

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
  CK_ASSERT_TRUE(only_block->IsTracked());

  freelist_->DeleteBlock(only_block->ToTracked());

  slab_manager_->Free(slab);
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
AllocatedBlock* LargeAllocatorImpl<SlabMap, SlabManager>::MakeBlockFromFreelist(
    uint64_t block_size, std::optional<size_t> alignment) {
  TrackedBlock* free_block =
      alignment.has_value()
          ? freelist_->FindFreeAligned(block_size, alignment.value())
          : freelist_->FindFree(block_size);
  if (free_block == nullptr) {
    return nullptr;
  }

  BlockedSlab* slab =
      slab_map_->FindSlab(PageId::FromPtr(free_block))->ToBlocked();

  AllocatedBlock* allocated_block;
  if (alignment.has_value()) {
    auto [prev_free, block, next_free] =
        freelist_->SplitAligned(free_block, block_size, alignment.value());
    allocated_block = block;
  } else {
    auto [block, remainder_block] = freelist_->Split(free_block, block_size);
    allocated_block = block;
  }

  slab->AddAllocation(allocated_block->Size());
  return allocated_block;
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
AllocatedBlock*
LargeAllocatorImpl<SlabMap, SlabManager>::AllocBlockedSlabAndMakeBlock(
    uint64_t block_size, std::optional<size_t> alignment) {
  CK_ASSERT_LT(alignment.value_or(0), kPageSize);

  size_t alignment_offset = 0;
  if (alignment.has_value()) {
    alignment_offset =
        AlignUpDiff(Block::kFirstBlockInSlabOffset + Block::kMetadataOverhead,
                    alignment.value());
  }

  uint32_t n_pages = BlockedSlab::NPagesForBlock(block_size + alignment_offset);
  auto result = slab_manager_->template Alloc<BlockedSlab>(n_pages);
  if (result == std::nullopt) {
    return nullptr;
  }
  BlockedSlab* slab = result->second;

  uint64_t remaining_block_size = slab->MaxBlockSize();
  Block* block = slab_manager_->FirstBlockInBlockedSlab(slab);
  bool prev_free = false;
  // If alignment forces this block to start somewhere past the beginning, we
  // need to initialize a free block at the beginning.
  if (alignment.has_value() && alignment_offset != 0) {
    CK_ASSERT_GE(alignment_offset, Block::kMinBlockSize);
    freelist_->InitFree(block, alignment_offset);

    block = block->NextAdjacentBlock();
    remaining_block_size -= alignment_offset;
    prev_free = true;
  }

  CK_ASSERT_LE(block_size, remaining_block_size);
  uint64_t remainder_size = remaining_block_size - block_size;
  CK_ASSERT_TRUE(IsAligned(remainder_size, kDefaultAlignment));

  AllocatedBlock* allocated_block = block->InitAllocated(block_size, prev_free);
  slab->AddAllocation(block_size);

  // Write a phony header for an allocated block of size 0 at the end of the
  // slab, which will trick the last block in the slab into never trying to
  // coalesce with its next adjacent neighbor.
  Block* slab_end_header = allocated_block->NextAdjacentBlock();

  if (remainder_size != 0) {
    Block* next_adjacent = slab_end_header;
    freelist_->InitFree(next_adjacent, remainder_size);

    slab_end_header = next_adjacent->NextAdjacentBlock();
    slab_end_header->InitPhonyHeader(/*prev_free=*/true);
  } else {
    slab_end_header->InitPhonyHeader(/*prev_free=*/false);
  }

  return allocated_block;
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
Void* LargeAllocatorImpl<SlabMap, SlabManager>::AllocSingleAllocSlab(
    size_t user_size, std::optional<size_t> alignment) {
  CK_ASSERT_TRUE(SingleAllocSlab::SizeSuitableForSingleAlloc(user_size) ||
                 alignment.value_or(0) >= kPageSize);
  uint32_t n_pages = SingleAllocSlab::NPagesForAlloc(user_size);
  auto result = alignment.has_value()
                    ? slab_manager_->template AlignedAlloc<SingleAllocSlab>(
                          n_pages, alignment.value())
                    : slab_manager_->template Alloc<SingleAllocSlab>(n_pages);
  if (result == std::nullopt) {
    return nullptr;
  }

  return reinterpret_cast<Void*>(result->first.PageStart());
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

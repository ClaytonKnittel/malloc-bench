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

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap,
          SlabManagerInterface SlabManager>
class MainAllocatorImpl {
  friend class MainAllocatorFixture;

 public:
  MainAllocatorImpl(SlabMap* slab_map, SlabManager* slab_manager)
      : slab_map_(slab_map), slab_manager_(slab_manager) {}

  // Allocates a region of memory `user_size` bytes long, returning a pointer to
  // the beginning of the region.
  void* Alloc(size_t user_size);

  // Re-allocates a region of memory to be `user_size` bytes long, returning a
  // pointer to the beginning of the new region and copying the data from `ptr`
  // over. The returned pointer may equal the `ptr` argument. If `user_size` is
  // larger than the previous size of the region starting at `ptr`, the
  // remaining data after the size of the previous region is uninitialized, and
  // if `user_size` is smaller, the data is truncated.
  void* Realloc(void* ptr, size_t user_size);

  // Frees an allocation returned from `Alloc`, allowing that memory to be
  // reused by future `Alloc`s.
  void Free(void* ptr);

 private:
  // Performs allocation for a large-sized allocation (i.e.
  // !IsSmallSize(user_size)).
  void* AllocLarge(size_t user_size);

  // Performs reallocation for an allocation in a large slab. Note that
  // `user_size` may not be a large size.
  void* ReallocLarge(LargeSlab* slab, void* ptr, size_t user_size);

  // Frees an allocation in a large slab.
  void FreeLarge(LargeSlab* slab, void* ptr);

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

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap,
          SlabManagerInterface SlabManager>
void* MainAllocatorImpl<MetadataAlloc, SlabMap, SlabManager>::Alloc(
    size_t user_size) {
  if (IsSmallSize(user_size)) {
    return nullptr;
  }

  return AllocLarge(user_size);
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap,
          SlabManagerInterface SlabManager>
void* MainAllocatorImpl<MetadataAlloc, SlabMap, SlabManager>::Realloc(
    void* ptr, size_t user_size) {
  Slab* slab = slab_map_->FindSlab(slab_manager_->PageIdFromPtr(ptr));
  CK_ASSERT_EQ(slab->Type(), SlabType::kLarge);

  switch (slab->Type()) {
    case SlabType::kSmall: {
      return nullptr;
    }
    case SlabType::kLarge: {
      return ReallocLarge(slab->ToLarge(), ptr, user_size);
    }
    case SlabType::kUnmapped:
    case SlabType::kFree: {
      // Unexpected free/unmapped slab.
      CK_ASSERT_EQ(slab->Type(), SlabType::kSmall);
      return nullptr;
    }
  }
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap,
          SlabManagerInterface SlabManager>
void MainAllocatorImpl<MetadataAlloc, SlabMap, SlabManager>::Free(void* ptr) {
  Slab* slab = slab_map_->FindSlab(slab_manager_->PageIdFromPtr(ptr));
  CK_ASSERT_EQ(slab->Type(), SlabType::kLarge);

  switch (slab->Type()) {
    case SlabType::kSmall: {
      break;
    }
    case SlabType::kLarge: {
      FreeLarge(slab->ToLarge(), ptr);
      break;
    }
    case SlabType::kUnmapped:
    case SlabType::kFree: {
      // Unexpected free/unmapped slab.
      CK_ASSERT_EQ(slab->Type(), SlabType::kSmall);
      break;
    }
  }
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap,
          SlabManagerInterface SlabManager>
void* MainAllocatorImpl<MetadataAlloc, SlabMap, SlabManager>::AllocLarge(
    size_t user_size) {
  AllocatedBlock* block = MakeBlockFromFreelist(user_size);

  // If allocating from the freelist fails, we need to request another slab of
  // memory.
  if (block == nullptr) {
    block = AllocLargeSlabAndMakeBlock(user_size);
  }

  return block != nullptr ? block->UserDataPtr() : nullptr;
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap,
          SlabManagerInterface SlabManager>
void* MainAllocatorImpl<MetadataAlloc, SlabMap, SlabManager>::ReallocLarge(
    LargeSlab* slab, void* ptr, size_t user_size) {
  AllocatedBlock* block = AllocatedBlock::FromUserDataPtr(ptr);
  uint64_t block_size = block->Size();
  uint64_t new_block_size = Block::BlockSizeForUserSize(user_size);

  // If we can resize the block in-place, then we don't need to copy any data
  // and can return the same pointer back to the user.
  if (freelist_.ResizeIfPossible(block, new_block_size)) {
    slab->AddAllocation(new_block_size);
    slab->RemoveAllocation(block_size);
    return ptr;
  }

  // Otherwise, if resizing in-place didn't work, then we have to allocate a new
  // block and copy the contents of this one over to the new one.
  void* new_ptr = Alloc(user_size);
  if (new_ptr != nullptr) {
    std::memcpy(new_ptr, ptr,
                std::min<size_t>(user_size, block->UserDataSize()));
    Free(ptr);
  }
  return new_ptr;
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap,
          SlabManagerInterface SlabManager>
void MainAllocatorImpl<MetadataAlloc, SlabMap, SlabManager>::FreeLarge(
    LargeSlab* slab, void* ptr) {
  AllocatedBlock* block = AllocatedBlock::FromUserDataPtr(ptr);
  slab->RemoveAllocation(block->Size());
  freelist_.MarkFree(block);

  if (slab->AllocatedBytes() == 0) {
    ReleaseLargeSlab(slab);
  }
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap,
          SlabManagerInterface SlabManager>
void MainAllocatorImpl<MetadataAlloc, SlabMap, SlabManager>::ReleaseLargeSlab(
    LargeSlab* slab) {
  CK_ASSERT_EQ(slab->AllocatedBytes(), 0);
  Block* only_block = slab_manager_->FirstBlockInLargeSlab(slab);
  CK_ASSERT_EQ(only_block->Size(), slab->MaxBlockSize());
  CK_ASSERT_TRUE(only_block->Free());
  CK_ASSERT_TRUE(!only_block->IsUntracked());

  freelist_.DeleteBlock(only_block->ToTracked());
  slab_manager_->Free(slab);
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap,
          SlabManagerInterface SlabManager>
AllocatedBlock*
MainAllocatorImpl<MetadataAlloc, SlabMap, SlabManager>::MakeBlockFromFreelist(
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

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap,
          SlabManagerInterface SlabManager>
AllocatedBlock*
MainAllocatorImpl<MetadataAlloc, SlabMap,
                  SlabManager>::AllocLargeSlabAndMakeBlock(size_t user_size) {
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

using MainAllocator =
    MainAllocatorImpl<GlobalMetadataAlloc, SlabMap, SlabManager>;

}  // namespace ckmalloc

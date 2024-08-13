#pragma once

#include <cstddef>
#include <cstring>

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/common.h"
#include "src/ckmalloc/freelist.h"
#include "src/ckmalloc/slab_manager.h"
#include "src/ckmalloc/slab_map.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap,
          SlabManagerInterface SlabManager>
class MainAllocatorImpl {
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

  // Frees a region of memory returned from `Alloc`, allowing that memory to be
  // reused by future `Alloc`s.
  void Free(void* ptr);

 private:
  // Tries to find a free block large enough for `user_size`, and if one is
  // found, returns the `AllocatedBlock` large enough to serve this request.
  AllocatedBlock* MakeBlockFromFreelist(size_t user_size);

  // Allocates a new large slab large enough for `user_size`, and returns a
  // pointer to the newly created `AllocatedBlock` that is large enough for
  // `user_size`.
  AllocatedBlock* AllocLargeSlabAndMakeBlock(size_t user_size);

  SlabMap* slab_map_;

  SlabManager* slab_manager_;

  Freelist freelist_;
};

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap,
          SlabManagerInterface SlabManager>
void* MainAllocatorImpl<MetadataAlloc, SlabMap, SlabManager>::Alloc(
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
void* MainAllocatorImpl<MetadataAlloc, SlabMap, SlabManager>::Realloc(
    void* ptr, size_t user_size) {
  void* new_ptr = Alloc(user_size);
  if (new_ptr != nullptr) {
    // TODO: copy min of sizes.
    std::memcpy(new_ptr, ptr, user_size);
  }
  Free(ptr);
  return new_ptr;
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap,
          SlabManagerInterface SlabManager>
void MainAllocatorImpl<MetadataAlloc, SlabMap, SlabManager>::Free(void* ptr) {
  AllocatedBlock* block = AllocatedBlock::FromUserDataPtr(ptr);
  freelist_.MarkFree(block);
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap,
          SlabManagerInterface SlabManager>
AllocatedBlock*
MainAllocatorImpl<MetadataAlloc, SlabMap, SlabManager>::MakeBlockFromFreelist(
    size_t user_size) {
  FreeBlock* free_block = freelist_.FindFree(user_size);
  if (free_block == nullptr) {
    return nullptr;
  }

  auto [allocated_block, remainder_block] =
      freelist_.Split(free_block, Block::BlockSizeForUserSize(user_size));
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

  // Write a phony header for an allocated block of size 0 at the end of the
  // slab, which will trick the last block in the slab from never trying to
  // coalesce with its next adjacent neighbor.
  Block* slab_end_header = block->NextAdjacentBlock();

  if (remainder_size != 0) {
    Block* next_adjacent = block->NextAdjacentBlock();
    if (Block::IsUntrackedSize(remainder_size)) {
      next_adjacent->InitUntracked(remainder_size);
    } else {
      freelist_.InitFree(next_adjacent, remainder_size);
    }

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

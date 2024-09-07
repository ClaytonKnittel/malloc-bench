#include "src/ckmalloc/freelist.h"

#include <utility>

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/common.h"
#include "src/ckmalloc/linked_list.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

TrackedBlock* Freelist::FindFree(size_t user_size) {
  TrackedBlock* best = nullptr;
  // TODO make bins for large sizes.
  for (TrackedBlock& block : free_blocks_) {
    if (block.UserDataSize() >= user_size &&
        (best == nullptr || block.UserDataSize() < best->UserDataSize())) {
      best = &block;
    }
  }

  return best;
}

std::pair<AllocatedBlock*, FreeBlock*> Freelist::InitializeSlabEnd(
    Block* block, uint64_t total_size, uint64_t alloc_size) {
  AllocatedBlock* allocated_block;
  Block* slab_end_header;
  if (alloc_size != 0) {
    allocated_block = block->InitAllocated(alloc_size, /*prev_free=*/false);
    slab_end_header = allocated_block->NextAdjacentBlock();
  } else {
    allocated_block = nullptr;
    slab_end_header = block;
  }

  // Write a phony header for an allocated block of size 0 at the end of the
  // slab, which will trick the last block in the slab into never trying to
  // coalesce with its next adjacent neighbor.
  FreeBlock* free_block;
  const uint64_t remainder_size =
      total_size - alloc_size - Block::kMetadataOverhead;
  if (remainder_size != 0) {
    Block* next_adjacent = slab_end_header;
    free_block = InitFree(next_adjacent, remainder_size);

    slab_end_header = next_adjacent->NextAdjacentBlock();
    slab_end_header->InitPhonyHeader(/*prev_free=*/true);
  } else {
    free_block = nullptr;
    slab_end_header->InitPhonyHeader(/*prev_free=*/false);
  }

  return std::make_pair(allocated_block, free_block);
}

FreeBlock* Freelist::InitFree(Block* block, uint64_t size) {
  CK_ASSERT_NE(size, 0);
  CK_ASSERT_TRUE(IsAligned(size, kDefaultAlignment));
  // Prev free is never true for free blocks, so we will not set that.
  block->header_ = size | Block::kFreeBitMask;
  block->WriteFooterAndPrevFree();

  if (!Block::IsUntrackedSize(size)) {
    InsertBlock(block->ToTracked());
  }

  return block->ToFree();
}

AllocatedBlock* Freelist::MarkAllocated(TrackedBlock* block) {
  // Remove ourselves from the freelist we are in.
  RemoveBlock(block);

  // Clear the free bit.
  block->header_ &= ~Block::kFreeBitMask;
  // Clear the prev-free bit of the next adjacent block.
  block->NextAdjacentBlock()->SetPrevFree(false);
  return block->ToAllocated();
}

std::pair<AllocatedBlock*, FreeBlock*> Freelist::Split(TrackedBlock* block,
                                                       uint64_t block_size) {
  uint64_t size = block->Size();
  CK_ASSERT_LE(block_size, size);

  uint64_t remainder = size - block_size;
  if (remainder == 0) {
    AllocatedBlock* allocated_block = MarkAllocated(block);
    return std::make_pair(allocated_block, nullptr);
  }

  block->SetSize(block_size);
  AllocatedBlock* allocated_block = MarkAllocated(block);

  FreeBlock* remainder_block =
      InitFree(allocated_block->NextAdjacentBlock(), remainder);
  return std::make_pair(allocated_block, remainder_block);
}

FreeBlock* Freelist::MarkFree(AllocatedBlock* block) {
  uint64_t size = block->Size();
  Block* block_start = block;
  if (block->PrevFree()) {
    Block* prev = block->PrevAdjacentBlock();
    CK_ASSERT_EQ(prev->Size(), block->PrevSize());
    size += block->PrevSize();

    if (!prev->IsUntracked()) {
      RemoveBlock(prev->ToTracked());
    }
    block_start = prev;
  }
  Block* next = block->NextAdjacentBlock();
  if (next->Free()) {
    size += next->Size();

    if (!next->IsUntracked()) {
      RemoveBlock(next->ToTracked());
    }
  }

  block_start->SetSize(size);
  block_start->header_ |= Block::kFreeBitMask;
  block_start->WriteFooterAndPrevFree();

  if (!Block::IsUntrackedSize(size)) {
    TrackedBlock* free_block = block_start->ToTracked();
    InsertBlock(free_block);
  }
  return block_start->ToFree();
}

bool Freelist::ResizeIfPossible(AllocatedBlock* block, uint64_t new_size) {
  uint64_t block_size = block->Size();
  Block* next_block = block->NextAdjacentBlock();
  uint64_t next_size = next_block->Size();

  // If new_size is smaller than block_size, then shrink this block in place.
  if (new_size <= block_size) {
    block->SetSize(new_size);
    Block* new_head = block->NextAdjacentBlock();

    if (next_block->Free()) {
      // If the next block is free, we can extend the block backwards.
      MoveBlockHeader(next_block->ToFree(), new_head,
                      next_size + block_size - new_size);
    } else if (new_size != block_size) {
      // Otherwise, we create a new free block in between the shrunk block and
      // next_block.
      InitFree(new_head, block_size - new_size);
    }
    return true;
  }

  if (next_block->Free() && new_size <= block_size + next_size) {
    block->SetSize(new_size);
    MoveBlockHeader(next_block->ToFree(), block->NextAdjacentBlock(),
                    next_size + block_size - new_size);
    return true;
  }

  return false;
}

void Freelist::InsertBlock(TrackedBlock* block) {
  free_blocks_.InsertFront(block);
}

void Freelist::DeleteBlock(TrackedBlock* block) {
  RemoveBlock(block);
}

void Freelist::RemoveBlock(TrackedBlock* block) {
  block->LinkedListNode::Remove();
}

void Freelist::MoveBlockHeader(FreeBlock* block, Block* new_head,
                               uint64_t new_size) {
  CK_ASSERT_EQ(
      static_cast<int64_t>(block->Size() - new_size),
      reinterpret_cast<int64_t>(new_head) - reinterpret_cast<int64_t>(block));

  if (!block->IsUntracked()) {
    RemoveBlock(block->ToTracked());
  }

  if (new_size != 0) {
    InitFree(new_head, new_size);
  } else {
    new_head->SetPrevFree(false);
  }
}

}  // namespace ckmalloc

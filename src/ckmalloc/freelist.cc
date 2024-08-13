#include "src/ckmalloc/freelist.h"

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/linked_list.h"

namespace ckmalloc {

FreeBlock* Freelist::FindFree(size_t user_size) {
  FreeBlock* best = nullptr;
  for (FreeBlock& block : free_blocks_) {
    // Take the first block that fits.
    if (block.UserDataSize() >= user_size) {
      if (best == nullptr) {
        best = &block;
      } else {
        return &block;
      }
    }
  }

  return best;
}

FreeBlock* Freelist::InitFree(Block* block, uint64_t size) {
  CK_ASSERT_GE(size, Block::kMinLargeSize);
  CK_ASSERT_TRUE(IsAligned(size, kDefaultAlignment));
  // Prev free is never true for free blocks, so we will not set that.
  block->header_ = size | Block::kFreeBitMask;
  block->WriteFooterAndPrevFree();

  FreeBlock* free_block = block->ToFree();
  AddBlock(free_block);
  return free_block;
}

AllocatedBlock* Freelist::MarkAllocated(FreeBlock* block) {
  // Remove ourselves from the freelist we are in.
  RemoveBlock(block);

  // Clear the free bit.
  block->header_ &= ~Block::kFreeBitMask;
  // Clear the prev-free bit of the next adjacent block.
  block->NextAdjacentBlock()->SetPrevFree(false);
  return block->ToAllocated();
}

std::pair<AllocatedBlock*, Block*> Freelist::Split(FreeBlock* block,
                                                   uint64_t block_size) {
  uint64_t size = block->Size();
  CK_ASSERT_LE(block_size, size);

  uint64_t remainder = size - block_size;
  // TODO: replace with remainder == 0
  if (remainder < Block::kMinBlockSize) {
    AllocatedBlock* allocated_block = MarkAllocated(block);
    return std::make_pair(allocated_block, nullptr);
  }

  block->SetSize(block_size);
  AllocatedBlock* allocated_block = MarkAllocated(block);

  Block* remainder_block = allocated_block->NextAdjacentBlock();
  if (Block::IsUntrackedSize(remainder)) {
    remainder_block->InitUntracked(remainder);
  } else {
    InitFree(remainder_block, remainder);
  }
  return std::make_pair(allocated_block, remainder_block);
}

FreeBlock* Freelist::MarkFree(AllocatedBlock* block) {
  uint64_t size = block->Size();
  Block* block_start = block;
  if (block->PrevFree()) {
    Block* prev = block->PrevAdjacentBlock();
    CK_ASSERT_EQ(prev->Size(), block->PrevSize());
    size += block->PrevSize();

    if (!prev->IsUntrackedSize()) {
      RemoveBlock(prev->ToFree());
    }
    block_start = prev;
  }
  Block* next = block->NextAdjacentBlock();
  if (next->Free()) {
    size += next->Size();

    if (!next->IsUntrackedSize()) {
      RemoveBlock(next->ToFree());
    }
  }

  block_start->SetSize(size);
  block_start->header_ |= Block::kFreeBitMask;
  block_start->WriteFooterAndPrevFree();

  FreeBlock* free_block = block_start->ToFree();
  AddBlock(free_block);
  return free_block;
}

void Freelist::AddBlock(FreeBlock* block) {
  free_blocks_.InsertFront(block);
}

void Freelist::RemoveBlock(FreeBlock* block) {
  block->LinkedListNode::Remove();
}

}  // namespace ckmalloc

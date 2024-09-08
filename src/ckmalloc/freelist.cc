#include "src/ckmalloc/freelist.h"

#include <cstddef>
#include <cstdint>

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/common.h"
#include "src/ckmalloc/linked_list.h"
#include "src/ckmalloc/red_black_tree.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

TrackedBlock* Freelist::FindFree(size_t user_size) {
  uint64_t block_size = Block::BlockSizeForUserSize(user_size);
  // If the required block size is small enough for the exact-size bins, check
  // those first in order of size, starting from `block_size`.
  if (block_size <= Block::kMaxExactSizeBlock) {
    for (auto it = exact_bin_skiplist_.begin(/*from=*/ExactSizeIdx(block_size));
         it != exact_bin_skiplist_.end(); ++it) {
      TrackedBlock* block = exact_size_bins_[*it].Front();
      if (block != nullptr) {
        return block;
      }

      // If this list was empty, clear the corresponding skiplist bit so we
      // don't check it again before filling it with something.
      it.ClearAt();
    }
  }

  return large_blocks_tree_.LowerBound(
      [user_size](const TreeBlock& tree_block) {
        return tree_block.UserDataSize() >= user_size;
      });
}

FreeBlock* Freelist::InitFree(Block* block, uint64_t size) {
  CK_ASSERT_NE(size, 0);
  CK_ASSERT_TRUE(IsAligned(size, kDefaultAlignment));
  // Prev free is never true for free blocks, so we will not set that.
  block->header_ = size | Block::kFreeBitMask;
  block->WriteFooterAndPrevFree();

  if (!Block::IsUntrackedSize(size)) {
    AddBlock(block->ToTracked());
  }

  return block->ToFree();
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

  AllocatedBlock* allocated_block =
      MarkAllocated(block, /*new_size=*/block_size);

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
    AddBlock(block_start->ToTracked());
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

void Freelist::DeleteBlock(TrackedBlock* block) {
  RemoveBlock(block);
}

/* static */
size_t Freelist::ExactSizeIdx(uint64_t block_size) {
  CK_ASSERT_GT(block_size, kMaxSmallSize);
  CK_ASSERT_LE(block_size, Block::kMaxExactSizeBlock);
  return (block_size - kMaxSmallSize - kDefaultAlignment) / kDefaultAlignment;
}

AllocatedBlock* Freelist::MarkAllocated(TrackedBlock* block,
                                        std::optional<uint64_t> new_size) {
  // Remove ourselves from the freelist we are in.
  RemoveBlock(block);

  // Clear the free bit.
  block->header_ &= ~Block::kFreeBitMask;
  // Update the size if requested.
  if (new_size.has_value()) {
    block->SetSize(new_size.value());
  }
  // Clear the prev-free bit of the next adjacent block.
  block->NextAdjacentBlock()->SetPrevFree(false);
  return block->ToAllocated();
}

void Freelist::AddBlock(TrackedBlock* block) {
  uint64_t block_size = block->Size();
  if (block_size <= Block::kMaxExactSizeBlock) {
    size_t idx = ExactSizeIdx(block_size);
    exact_size_bins_[idx].InsertFront(block->ToExactSize());
    exact_bin_skiplist_.Set(idx);
  } else {
    new (static_cast<RbNode*>(block->ToTree())) RbNode();
    large_blocks_tree_.Insert(block->ToTree());
  }
}

void Freelist::RemoveBlock(TrackedBlock* block) {
  if (block->Size() <= Block::kMaxExactSizeBlock) {
    block->ToExactSize()->LinkedListNode::Remove();
  } else {
    large_blocks_tree_.Remove(block->ToTree());
  }
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

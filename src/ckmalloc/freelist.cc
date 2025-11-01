#include "src/ckmalloc/freelist.h"

#include <cstddef>
#include <cstdint>
#include <tuple>

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/common.h"
#include "src/ckmalloc/linked_list.h"
#include "src/ckmalloc/red_black_tree.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

namespace {

bool CanFitAlignedAlloc(Block* block, size_t block_size, size_t alignment) {
  return AlignUpDiff(reinterpret_cast<size_t>(
                         static_cast<AllocatedBlock*>(block)->UserDataPtr()),
                     alignment) +
             block_size <=
         block->Size();
}

}  // namespace

TrackedBlock* Freelist::FindFree(size_t block_size) {
  CK_ASSERT_TRUE(IsAligned(block_size, kDefaultAlignment));

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
      [block_size](const TreeBlock& tree_block) {
        return tree_block.Size() >= block_size;
      });
}

TrackedBlock* Freelist::FindFreeAligned(size_t block_size, size_t alignment) {
  // If the required block size is small enough for the exact-size bins, check
  // those first in order of size, starting from `block_size`.
  if (block_size <= Block::kMaxExactSizeBlock) {
    for (auto it = exact_bin_skiplist_.begin(/*from=*/ExactSizeIdx(block_size));
         it != exact_bin_skiplist_.end(); ++it) {
      LinkedList<ExactSizeBlock>& bin = exact_size_bins_[*it];
      for (ExactSizeBlock& block : bin) {
        if (CanFitAlignedAlloc(&block, block_size, alignment)) {
          return &block;
        }
      }
    }
  }

  TreeBlock* block =
      large_blocks_tree_.LowerBound([block_size](const TreeBlock& tree_block) {
        return tree_block.Size() >= block_size;
      });
  for (; block != nullptr && !CanFitAlignedAlloc(block, block_size, alignment);
       block = large_blocks_tree_.Next(block))
    ;

  return block;
}

TrackedBlock* Freelist::FindFreeLazy(size_t block_size) {
  CK_ASSERT_TRUE(IsAligned(block_size, kDefaultAlignment));

  // If the required block size is small enough for the exact-size bins, check
  // those first in order of size, starting from `block_size`.
  if (block_size <= Block::kMaxExactSizeBlock) {
    auto it = exact_bin_skiplist_.begin(/*from=*/ExactSizeIdx(block_size));
    if (it != exact_bin_skiplist_.end()) {
      return exact_size_bins_[*it].Front();
    }
  }

  return nullptr;
}

TrackedBlock* Freelist::FindFreeLazyAligned(size_t block_size,
                                            size_t alignment) {
  TrackedBlock* block = FindFreeLazy(block_size);
  return block != nullptr && CanFitAlignedAlloc(block, block_size, alignment)
             ? block
             : nullptr;
}

FreeBlock* Freelist::InitFree(Block* block, size_t size) {
  CK_ASSERT_GE(size, Block::kMinBlockSize);
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
                                                       size_t block_size) {
  size_t size = block->Size();
  CK_ASSERT_LE(block_size, size);

  size_t remainder = size - block_size;
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

std::tuple<FreeBlock*, AllocatedBlock*, FreeBlock*> Freelist::SplitAligned(
    TrackedBlock* block, size_t block_size, size_t alignment) {
  size_t size = block->Size();
  CK_ASSERT_LE(block_size, size);

  size_t alignment_offset =
      AlignUpDiff(reinterpret_cast<size_t>(
                      static_cast<AllocatedBlock*>(static_cast<Block*>(block))
                          ->UserDataPtr()),
                  alignment);
  if (alignment_offset == 0) {
    auto [alloc_block, free_block] = Split(block, block_size);
    return std::make_tuple(nullptr, alloc_block, free_block);
  }

  RemoveBlock(block);
  FreeBlock* prev_free = InitFree(block, alignment_offset);
  Block* middle_block = prev_free->NextAdjacentBlock();
  size -= alignment_offset;

  size_t remainder = size - block_size;
  if (remainder == 0) {
    AllocatedBlock* allocated_block = MarkAllocated(block);
    return std::make_tuple(prev_free, allocated_block, nullptr);
  }

  AllocatedBlock* allocated_block =
      middle_block->InitAllocated(block_size, /*prev_free=*/true);

  FreeBlock* next_free =
      InitFree(allocated_block->NextAdjacentBlock(), remainder);
  return std::make_tuple(prev_free, allocated_block, next_free);
}

FreeBlock* Freelist::MarkFree(AllocatedBlock* block) {
  size_t size = block->Size();
  Block* block_start = block;
  if (block->PrevFree()) {
    Block* prev = block->PrevAdjacentBlock();
    CK_ASSERT_EQ(prev->Size(), block->PrevSize());
    size += block->PrevSize();

    if (prev->IsTracked()) {
      RemoveBlock(prev->ToTracked());
    }
    block_start = prev;
  }
  Block* next = block->NextAdjacentBlock();
  if (next->Free()) {
    size += next->Size();

    if (next->IsTracked()) {
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

bool Freelist::ResizeIfPossible(AllocatedBlock* block, size_t new_size) {
  size_t block_size = block->Size();
  Block* next_block = block->NextAdjacentBlock();
  size_t next_size = next_block->Size();

  // If new_size is smaller than block_size, then shrink this block in place.
  if (new_size <= block_size) {
    block->SetSize(new_size);
    Block* new_head = block->NextAdjacentBlock();

    if (next_block->Free()) {
      // If the next block is free, we can extend the block backwards.
      FreeBlock* next_free = next_block->ToFree();
      if (next_free->IsTracked()) {
        RemoveBlock(next_free->ToTracked());
      }

      InitFree(new_head, next_size + block_size - new_size);
    } else if (block_size != new_size) {
      // Otherwise, we create a new free block in between the shrunk block and
      // next_block.
      InitFree(new_head, block_size - new_size);
    }
    return true;
  }

  if (next_block->Free() && new_size <= block_size + next_size) {
    if (next_block->IsTracked()) {
      RemoveBlock(next_block->ToTracked());
    }

    size_t remainder_size = block_size + next_size - new_size;
    if (remainder_size == 0) {
      block->SetSize(block_size + next_size);
      block->NextAdjacentBlock()->SetPrevFree(false);
    } else {
      block->SetSize(new_size);
      InitFree(block->NextAdjacentBlock(), remainder_size);
    }
    return true;
  }

  return false;
}

void Freelist::DeleteBlock(TrackedBlock* block) {
  RemoveBlock(block);
}

/* static */
size_t Freelist::ExactSizeIdx(size_t block_size) {
  CK_ASSERT_GE(block_size, Block::kMinTrackedSize);
  CK_ASSERT_LE(block_size, Block::kMaxExactSizeBlock);
  return (block_size - Block::kMinTrackedSize) / kDefaultAlignment;
}

AllocatedBlock* Freelist::MarkAllocated(TrackedBlock* block,
                                        std::optional<size_t> new_size) {
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
  size_t block_size = block->Size();
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

}  // namespace ckmalloc

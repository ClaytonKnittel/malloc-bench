#include "src/pkmalloc/free_block.h"

namespace pkmalloc {

void FreeBlock::SetNext(FreeBlock* first, FreeBlock* second) {
  first->next_ = second;
}

FreeBlock* FreeBlock::GetNext(FreeBlock* current_block) {
  return current_block->next_;
}

void FreeBlock::RemoveNext(FreeBlock* current, FreeBlock* next) {
  SetNext(current, GetNext(next));
}

FreeBlock* FreeBlock::combine(FreeBlock* left_block, FreeBlock* right_block) {
  // Do I need to track that address left < address right?
  // this should happen in free list::add_free_block_to_list
  left_block->SetFree(true);
  left_block->SetBlockSize(left_block->GetBlockSize() +
                           right_block->GetBlockSize());
  RemoveNext(left_block, right_block);
  left_block->next_ = right_block->next_;
  return left_block;
}

void FreeBlock::coalesce(FreeBlock* current, FreeBlock* prev) {
  // check in both directions for free blocks, if free, combine
  if (prev != nullptr) {
    if (prev->IsFree()) {
      current = combine(prev, current);
    }
  }
  if (current->next_ != nullptr) {
    if (current->next_->IsFree()) {
      current = combine(current, current->next_);
    }
  }
}

}  // namespace pkmalloc
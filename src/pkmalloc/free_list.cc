#include "src/pkmalloc/free_list.h"

FreeBlock* FreeList::add_free_block_to_list(AllocatedBlock* curr_block) {
  FreeBlock* current_block = FreeBlock::alloc_to_free(curr_block);
  auto* free_list_iter = begin_;
  auto* prev_iter = begin_;
  // free list is empty
  if (free_list_iter == nullptr) {
    begin_ = current_block;
  }
  // current is adress first in free list
  else if (&current_block < &free_list_iter) {
    begin_ = current_block;
    FreeBlock::SetNext(current_block, free_list_iter);
  }
  free_list_iter = FreeBlock::GetNext(free_list_iter);
  // current address is both second and last
  if (free_list_iter == nullptr) {
    FreeBlock::SetNext(prev_iter, current_block);
    FreeBlock::SetNext(current_block, free_list_iter);
  }
  while (free_list_iter != nullptr) {
    if (&current_block < &free_list_iter) {
      FreeBlock::SetNext(prev_iter, current_block);
      FreeBlock::SetNext(current_block, free_list_iter);
    }
  }
}

#include "src/pkmalloc/free_list.h"

#include "src/heap_factory.h"
#include "src/heap_interface.h"
#include "src/pkmalloc/global_state.h"
#include "src/pkmalloc/pkmalloc.h"

namespace pkmalloc {

void FreeList::AddFreeBlockToList(AllocatedBlock* curr_block,
                                  GlobalState* global_state) {
  FreeBlock* current_block = AllocatedBlock::AllocToFree(curr_block);
  FreeBlock* begin = global_state->GlobalState::GetFreeListStart(global_state);
  FreeBlock* free_list_iter = begin;
  FreeBlock* prev_iter = free_list_iter;
  // free list is empty
  if (free_list_iter == nullptr) {
    begin = current_block;
  }
  // CALL COALESCE IN HERE ?????????????????
  // current is adress first in free list
  else if (&current_block < &free_list_iter) {
    begin = current_block;
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

AllocatedBlock* FreeList::EmptyFreeListAlloc(size_t size,
                                             GlobalState* global_state) {
  AllocatedBlock* start_block =
      AllocatedBlock::CreateBlockExtendHeap(size, global_state);
  return start_block;
}

AllocatedBlock* FreeList::FindFreeBlockForAlloc(size_t size,
                                                FreeBlock* free_list_start,
                                                GlobalState* global_state) {
  auto* begin = free_list_start;
  auto* current_block = begin;
  AllocatedBlock* result_block;
  // search free list for free memory of size size
  while (current_block != nullptr) {
    // shouldn't need to check if free block from free list is free
    if (current_block->IsFree()) {
      // IS HEADER SIZE INCLUDED/CONSIDERED HERE?
      if (current_block->GetBlockSize() >= size) {
        result_block = AllocatedBlock::FreeToAlloc(current_block);
        return result_block;
      }
    }
    current_block = FreeBlock::GetNext(current_block);
  }
  // if nothing is found in free list of a big enough size, extend heap
  result_block = AllocatedBlock::CreateBlockExtendHeap(size, global_state);
  return result_block;
}

AllocatedBlock* FreeList::mallocate(size_t size, GlobalState* global_state) {
  FreeBlock* free_list_start =
      global_state->GlobalState::GetFreeListStart(global_state);
  if (free_list_start == nullptr) {
    return EmptyFreeListAlloc(size, global_state);
  }
  return FindFreeBlockForAlloc(size, free_list_start, global_state);
}

}  // namespace pkmalloc
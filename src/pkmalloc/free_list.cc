#include "src/pkmalloc/free_list.h"

#include "src/heap_factory.h"
#include "src/heap_interface.h"

namespace pkmalloc {

void FreeList::add_free_block_to_list(AllocatedBlock* curr_block,
                                      FreeBlock* begin) {
  FreeBlock* current_block = AllocatedBlock::alloc_to_free(curr_block);
  auto* free_list_iter = begin;
  auto* prev_iter = begin;
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

static AllocatedBlock* EmptyFreeListAlloc(size_t size) {
  // to align, keep?????????????
  // bench::Heap::GlobalInstance()->sbrk(8);
  AllocatedBlock* start_block = AllocatedBlock::create_block_extend_heap(size);
  // AllocatedBlock* start_block = bench::
  return start_block;
}

AllocatedBlock* FindFreeBlockForAlloc(size_t size, FreeBlock* free_list_start) {
  // auto* start_block = reinterpret_cast<FreeBlock*>(
  // reinterpret_cast<uint8_t*>(SingletonHeap::GlobalInstance()->Start()) + 8);
  auto* begin = free_list_start;
  auto* current_block = begin;
  AllocatedBlock* result_block;
  // auto* end_block =
  // reinterpret_cast<FreeBlock*>(SingletonHeap::GlobalInstance()->End());
  // search current heap for free memory of size size
  while (current_block != nullptr) {
    if (current_block->IsFree()) {
      if (current_block->GetBlockSize() >= size) {
        result_block = AllocatedBlock::free_to_alloc(current_block);
        return result_block;
      }
    }
    current_block = FreeBlock::GetNext(current_block);
  }
  // what are these next two lines for? why do i need temp
  // auto* temp = current_block;
  // current_block = FreeBlock::GetNext(current_block);
  // MALLOC_ASSERT(current_block > temp);
  // need to increase heap size for this call
  // could be current block but must assert this equals end block???? maybe
  // doesnt matter
  result_block = AllocatedBlock::create_block_extend_heap(size);
  return result_block;
}

AllocatedBlock* mallocate(size_t size, FreeBlock* free_list_start) {
  if (free_list_start == nullptr) {
    return EmptyFreeListAlloc(size);
  }
  return FindFreeBlockForAlloc(size, free_list_start);
}
}  // namespace pkmalloc
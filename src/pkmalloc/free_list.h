#pragma once

#include "global_state.h"
#include "src/heap_factory.h"
#include "src/heap_interface.h"
#include "src/pkmalloc/allocated_block.h"
#include "src/pkmalloc/free_block.h"

namespace pkmalloc {

class FreeList {
 public:
  // frees allocated block and adds its space to the free list
  static void AddFreeBlockToList(AllocatedBlock* curr_block,
                                 GlobalState* global_state);

  // free list is currently empty, heap factory returns alloc block
  static AllocatedBlock* EmptyFreeListAlloc(size_t size,
                                            GlobalState* global_state);

  // searches current free list for space to alloc for user, reallocs if needed
  static AllocatedBlock* FindFreeBlockForAlloc(size_t size,
                                               FreeBlock* free_list_begin,
                                               GlobalState* global_state);

  // returns allocated block of user requested size on the heap
  static AllocatedBlock* mallocate(size_t size, GlobalState* global_state);
};

}  // namespace pkmalloc
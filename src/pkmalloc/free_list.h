#pragma once

#include "src/heap_factory.h"
#include "src/heap_interface.h"
#include "src/pkmalloc/allocated_block.h"
#include "src/pkmalloc/free_block.h"

namespace pkmalloc {
class FreeList {
 public:
  // frees allocated block and adds its space to the free list
  static void add_free_block_to_list(AllocatedBlock* curr_block,
                                     FreeBlock* begin);

  // free list is currently empty, heap factory returns alloc block
  static AllocatedBlock* EmptyFreeListAlloc(size_t size);

  // searches current free list for space to alloc for user, reallocs if needed
  static AllocatedBlock* FindFreeBlockForAlloc(size_t size,
                                               FreeBlock* free_list_begin);

  // returns allocated block of user requested size on the heap
  static AllocatedBlock* mallocate(size_t size, FreeBlock* free_list_start);

  // FreeBlock* GetBegin(Freelist* block) {
  //   return block->begin_;
  // }

  // void SetBegin(FreeBlock* begin_block) {
  //   begin_ = begin_block;
  // }

 private:
  // FreeBlock* begin_;
};

}  // namespace pkmalloc
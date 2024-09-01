#pragma once

#include "src/pkmalloc/allocated_block.h"
#include "src/pkmalloc/free_block.h"

class FreeList {
 public:
  // frees allocated block and adds its space to the free list
  static void add_free_block_to_list(AllocatedBlock* curr_block,
                                     FreeBlock* begin);

  // heap is currently empty, sbrk's space and gives alloc block
  static AllocatedBlock* EmptyHeapAlloc(size_t size);

  // searches current free list for space to alloc for user, reallocs if needed
  static AllocatedBlock* FindFreeBlockForAlloc(size_t size,
                                               FreeBlock* free_list_begin);

  // returns allocated block of user requested size on the heap
  static AllocatedBlock* mallocate(size_t size, FreeBlock* free_list_begin);

  // FreeBlock* GetBegin(Freelist* block) {
  //   return block->begin_;
  // }

  // void SetBegin(FreeBlock* begin_block) {
  //   begin_ = begin_block;
  // }

 private:
  // FreeBlock* begin_;
};
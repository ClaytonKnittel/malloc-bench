#pragma once

#include "src/pkmalloc/allocated_block.h"
#include "src/pkmalloc/block.h"
#include "src/pkmalloc/free_block.h"

class FreeList {
 public:
  // frees allocated block and adds its space to the free list
  FreeBlock* add_free_block_to_list(AllocatedBlock* curr_block);

 private:
  // How to store this info? maybe make a meta struct item
  FreeBlock* begin_;
  size_t heap_metadata_size_;
};
#pragma once

#include "src/pkmalloc/allocated_block.h"
#include "src/pkmalloc/block.h"
#include "src/pkmalloc/free_block.h"

class FreeList {
 public:
  // initializes heap metedata structure and returns a pointer to it on the heap
  FreeList* create_free_list_structure();

  // changes heap metadata structure to align with current state of heap
  FreeList* edit_free_list_structure();

  // allocate more memory for heap metadata structure when it runs out of room
  FreeList* realloc_free_list_structure();

  // doubly linked list

 private:
  // How to store this info? maybe make a meta struct item
  FreeBlock* begin_;
  size_t heap_metadata_size_;
};
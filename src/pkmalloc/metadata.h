#pragma once

#include "src/pkmalloc/allocated_block.h"
#include "src/pkmalloc/block.h"
#include "src/pkmalloc/free_block.h"

class Metadata {
 public:
  Metadata* create_metadata_structure();
  // have a pointer to this structure
  // which will be stored on the heap

  // this will hold all the free blocks
  // and their size

  //

 private:
  // How to store this info? maybe make a metata struct item
  FreeBlock* top_value_;
  size_t size_;
};
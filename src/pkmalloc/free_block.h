#pragma once

#include "src/pkmalloc/block.h"
#include "src/pkmalloc/free_list.h"

class FreeBlock : public Block {
 public:
  // combine adjacent free blocks to be one big free block
  void coalesce();

  // helper to coalesce
  void combine(FreeList* left, FreeList* right) {}

  // data structure of pointers to free blocks
  // look at programming restrictions in spec
  // in normal block class, need some type of manager to flag things as free
  // blocks and do stuff
  // sort free blocks by size or address? how to coalesce efficiently and have
  // quick lookup for certain size memory chunk
  // eventually have special cases for smaller blocks (8-byte or less)
};
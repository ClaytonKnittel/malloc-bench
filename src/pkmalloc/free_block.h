#pragma once

#include "src/pkmalloc/block.h"

class FreeBlock : public Block {
 public:
  Block* FromRawPtr(void* ptr);
  // data structure of pointers to free blocks

  // look at programming restrictions in spec

  // in normal block class, need some type of manager to flag things as free
  // blocks and do stuff

  // coalesce adjacent free blocks

  // sort free blocks by size or address? how to coalesce efficiently and have
  // quick lookup for certain size memory chunk

  // eventually have special cases for smaller blocks (8-byte or less)
};
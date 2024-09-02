#pragma once

#include "src/pkmalloc/block.h"

class FreeBlock : public Block {
 public:
  // sets the current block's next to be next value
  static void SetNext(FreeBlock* first, FreeBlock* second);

  // returns the next block relative to the current block
  static FreeBlock* GetNext(FreeBlock* current_block);

  // sets the current block's next to be next's next value in coalescing
  static void RemoveNext(FreeBlock* current_block, FreeBlock* next);

  // helper to coalesce
  static FreeBlock* combine(FreeBlock* left_block, FreeBlock* right_block);

  // combine adjacent free blocks to be one big free block
  static void coalesce(FreeBlock* current, FreeBlock* prev);

  // look at programming restrictions in spec
  // how to coalesce efficiently and have
  // quick lookup for certain size memory chunk
  // eventually have special cases for smaller blocks (8-byte or less)

 private:
  FreeBlock* next_;
};
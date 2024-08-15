#pragma once

#include "src/pkmalloc/block.h"

class AllocatedBlock : public Block {
 public:
  // returns pointer to beginning of user information in a memory block
  uint8_t* GetBody();

  // changes type of void pointer to be uint8_t pointer
  static AllocatedBlock* FromRawPtr(void* ptr);

  // initializes free block to now be allocated and
  // returns pointer to beginning of this now allocated block
  AllocatedBlock* take_free_block();

  // extends the heap and initializes a new allocated block
  static AllocatedBlock* create_block_extend_heap(size_t size);

  // give the size needed for an allocated block to be 16 byte alligned and
  // including header size
  static size_t space_needed_with_header(const size_t& size);

 private:
  // flexible size, returns pointer to beginning of body
  // this goes in allocated block
  uint8_t body_[];
};
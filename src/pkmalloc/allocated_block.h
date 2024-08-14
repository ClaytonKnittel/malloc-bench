#pragma once

#include "src/pkmalloc/block.h"

class AllocatedBlock : public Block {
 public:
  uint8_t* GetBody();

  static AllocatedBlock* FromRawPtr(void* ptr);

  // Block* take_free_block(size_t size) {
  AllocatedBlock* take_free_block();

  void SetBlockSize(uint64_t size);

  static AllocatedBlock* create_block_extend_heap(size_t size);

  static size_t space_needed_with_header(const size_t& size);

 private:
  // flexible size, returns pointer to beginning of body
  // this goes in allocated block
  uint8_t body_[];
};
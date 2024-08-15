#pragma once

#include <cassert>

#include "src/singleton_heap.h"

class Block {
 public:
  // gets the size of the block including the header size
  uint64_t GetBlockSize() const;

  // gets the size of the block excluding the header size
  uint64_t GetUserSize() const;

  // checks if block is free or allocated
  bool IsFree() const;

  // switches free state of a block to be opposite of what it currently is
  void SetFree(bool free);

  // sets magic value to be default value
  void SetMagic();

  // checks that indexing is valid by comparing that magic value is correct
  void CheckValid() const;

  // increments to adjacent blocks in memory in increasing order
  Block* GetNextBlock();

  // creates 16 byte aligned block size based upon requested size
  // (appropriately rounds up user param size and adds header size)
  void SetBlockSize(uint64_t size);

 private:
  uint64_t header_;
  uint64_t magic_value_ = 123456;
  uint64_t garbage_ = -0;
};
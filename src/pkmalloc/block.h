#pragma once

#include <cassert>

#include "src/singleton_heap.h"

class Block {
 public:
  uint64_t GetBlockSize() const;

  uint64_t GetUserSize() const;

  bool IsFree() const;

  void SetFree(bool free);

  void SetMagic();

  void CheckValid() const;

  Block* GetNextBlock();

 private:
  uint64_t header_;
  uint64_t magic_value_ = 123456;
  uint64_t garbage_ = -0;
};
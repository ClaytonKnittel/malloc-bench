#pragma once

#include <cstddef>

#include "src/jsmalloc/blocks/free_block.h"

namespace jsmalloc {
namespace blocks {

class RbTreeFreeList {
 public:
  FreeBlock* FindBestFit(size_t size);
  void Remove(FreeBlock* block);
  void Insert(FreeBlock* block);

 private:
  FreeBlock::Tree rbtree_;
};

}  // namespace blocks
}  // namespace jsmalloc

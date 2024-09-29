#pragma once

#include <cstddef>

#include "src/jsmalloc/blocks/block.h"
#include "src/jsmalloc/blocks/free_block.h"

namespace jsmalloc {
namespace blocks {

class LargeBlock {
 public:
  static size_t BlockSize(size_t data_size);

  static LargeBlock* Init(FreeBlock* block, size_t alignment = 1);

  /** Returns this block's size. */
  size_t BlockSize() const;

  /** The amount of data this block can allocate. */
  size_t DataSize() const;

  /** Pointer to the data stored by this block. */
  void* Data();

  static LargeBlock* FromDataPtr(void* ptr);

  BlockHeader* Header() { 
    return &header_;
  }

 private:
  explicit LargeBlock(size_t block_size, bool prev_block_is_free);

  BlockHeader header_;
  int32_t data_offset_;
  alignas(16) uint8_t data_[];
};

}  // namespace blocks
}  // namespace jsmalloc

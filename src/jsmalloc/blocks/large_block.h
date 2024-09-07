#pragma once

#include <cstddef>

#include "src/jsmalloc/blocks/block.h"
#include "src/jsmalloc/blocks/free_block.h"
#include "src/jsmalloc/blocks/free_block_allocator.h"

namespace jsmalloc {
namespace blocks {

class LargeBlockHelper;

class LargeBlock {
  friend LargeBlockHelper;

 public:
  static size_t BlockSize(size_t data_size);
  static LargeBlock* Init(FreeBlock* block);

  /** Returns this block's size. */
  size_t BlockSize() const;

  /** The amount of data this block can allocate. */
  size_t DataSize() const;

  /** Pointer to the data stored by this block. */
  void* Data();

 private:
  explicit LargeBlock(size_t block_size, bool prev_block_is_free);

  BlockHeader header_;
  [[maybe_unused]] uint8_t alignment_[8];
  DataPreamble data_preamble_;
  uint8_t data_[];
};

}  // namespace blocks
}  // namespace jsmalloc

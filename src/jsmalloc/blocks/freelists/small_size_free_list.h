#pragma once

#include <cstddef>

#include "src/jsmalloc/blocks/free_block.h"
#include "src/jsmalloc/util/bitset.h"

namespace jsmalloc {
namespace blocks {

class SmallSizeFreeList {
 public:
  SmallSizeFreeList();

  FreeBlock* FindBestFit(size_t size);
  void Remove(FreeBlock* block);
  void Insert(FreeBlock* block);

  /** Maximum block size supported by this free list. */
  static constexpr size_t kMaxSize = 8112;

 private:
  static constexpr size_t kBytesPerExactSizeBin = 16;
  static constexpr size_t kExactSizeBins = kMaxSize / kBytesPerExactSizeBin + 1;

  BitSet<kExactSizeBins> empty_exact_size_lists_;
  FreeBlock::List exact_size_lists_[kExactSizeBins + 1];
};

}  // namespace blocks
}  // namespace jsmalloc

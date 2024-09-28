#pragma once

#include <bit>
#include <cstddef>

#include "src/jsmalloc/blocks/free_block.h"

namespace jsmalloc {
namespace blocks {

template <uint32_t N>
concept SupportsFastMultiply = std::popcount(N) == 1;

class LearnedSizeFreeList {
 public:
  FreeBlock* FindBestFit(size_t size);
  bool MaybeRemove(FreeBlock* block);
  bool MaybeInsert(FreeBlock* block);

  size_t size = 0;

 private:
  static constexpr size_t kMinSampleSize = 256;

  struct Bin {
    int32_t count;
    size_t block_size;
    bool is_size_locked;
    FreeBlock::List free_blocks;
  };

  Bin* FindBin(size_t size);
  void RecordAllocation(Bin* bin, size_t size);

  size_t total_count_ = 0;

  static constexpr size_t kBinsLen = 16;
  static_assert(SupportsFastMultiply<kBinsLen>);
  Bin bins_[kBinsLen];
};

}  // namespace blocks
}  // namespace jsmalloc

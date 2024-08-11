#include "src/jsmalloc/blocks/small_block.h"

#include <bit>
#include <cstddef>

#include "src/jsmalloc/allocator.h"
#include "src/jsmalloc/blocks/block.h"
#include "src/jsmalloc/util/assert.h"
#include "src/jsmalloc/util/math.h"

namespace jsmalloc {
namespace blocks {

class SmallBlockHelper {
  static_assert(offsetof(SmallBlock, bins_) % 16 == 12);
  static_assert(offsetof(SmallBlock::Bin, data_) % 16 == 4);
};

SmallBlock* SmallBlock::New(Allocator& allocator, size_t data_size,
                            size_t bin_count) {
  size_t block_size =
      math::round_16b(offsetof(SmallBlock, bins_) +
                      (offsetof(Bin, data_) + data_size) * bin_count);
  void* ptr = allocator.Allocate(block_size);
  if (ptr == nullptr) {
    return nullptr;
  }
  return new (ptr) SmallBlock(block_size, data_size, bin_count);
}

size_t SmallBlock::BlockSize() const {
  return header_.BlockSize();
}

size_t SmallBlock::BinSize() const {
  return data_size_ + offsetof(Bin, data_);
}

int SmallBlock::FreeBinIndex() const {
  return std::countr_zero(free_bins_bitset_);
}

bool SmallBlock::IsFull() const {
  return free_bins_bitset_ == 0;
}

bool SmallBlock::IsEmpty() const {
  return std::popcount(free_bins_bitset_) == bin_count_;
}

uint32_t SmallBlock::DataOffsetForBinIndex(int index) const {
  return offsetof(SmallBlock, bins_) + BinSize() * index + offsetof(Bin, data_);
}

int SmallBlock::BinIndexForDataOffset(uint32_t offset) const {
  return (offset - offsetof(SmallBlock, bins_) - offsetof(Bin, data_)) /
         BinSize();
}

void* SmallBlock::Alloc() {
  DCHECK(!IsFull(), "Alloc() called when IsFull()=true.");

  int free_bin_idx = FreeBinIndex();
  MarkBinUsed(free_bin_idx);

  auto* bin = reinterpret_cast<Bin*>(&bins_[BinSize() * free_bin_idx]);
  bin->data_preamble_.offset = DataOffsetForBinIndex(free_bin_idx);
  return static_cast<void*>(bin->data_);
}

void SmallBlock::Free(void* ptr) {
  DCHECK(reinterpret_cast<BlockHeader*>(this) == BlockHeader::FromDataPtr(ptr),
         "Wrong ptr given to SmallBlock::Free");

  DataPreamble* data_preamble = DataPreambleFromDataPtr(ptr);
  uint32_t bin_idx = BinIndexForDataOffset(data_preamble->offset);
  MarkBinFree(bin_idx);
}

void SmallBlock::MarkBinFree(int index) {
  free_bins_bitset_ |= (static_cast<uint64_t>(1) << index);
}

void SmallBlock::MarkBinUsed(int index) {
  free_bins_bitset_ &= ~(static_cast<uint64_t>(1) << index);
}

size_t SmallBlock::DataSize() const {
  return data_size_;
}

SmallBlock::SmallBlock(size_t block_size, size_t data_size, size_t bin_count)
    : header_(block_size, BlockKind::kSmall),
      data_size_(data_size),
      bin_count_(bin_count),
      free_bins_bitset_(static_cast<uint64_t>(~0) >>
                        (sizeof(free_bins_bitset_) * 8 - bin_count)) {
  DCHECK_EQ(block_size % 16, 0);
  DCHECK_EQ(data_size_ % 16, 12);
  DCHECK_LE(bin_count_, sizeof(free_bins_bitset_) * 8);
  DCHECK_GT(bin_count_, 0);
  DCHECK_EQ(std::popcount(free_bins_bitset_), bin_count_);
}

}  // namespace blocks
}  // namespace jsmalloc

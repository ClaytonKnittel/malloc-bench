#include "src/jsmalloc/blocks/small_block.h"

#include <cstddef>

#include "src/jsmalloc/util/assert.h"
#include "src/jsmalloc/util/math.h"

namespace jsmalloc {
namespace blocks {

SmallBlock* SmallBlock::Init(void* block, size_t bin_size, size_t bin_count) {
  auto* small_block = new (block) SmallBlock(bin_size, bin_count);
  BitSet::Init(small_block->data_, small_block->bin_count_);

  return small_block;
}

size_t SmallBlock::BinSize() const {
  return bin_size_;
}

int SmallBlock::FreeBinIndex() const {
  return UsedBinBitSet()->countr_one();
}

bool SmallBlock::IsFull() const {
  return used_bin_count_ == bin_count_;
}

bool SmallBlock::IsEmpty() const {
  return used_bin_count_ == 0;
}

void* SmallBlock::DataPtrForBinIndex(int index) {
  return &DataRegion()[index * bin_size_];
}

int SmallBlock::BinIndexForDataPtr(void* ptr) const {
  return (reinterpret_cast<uint8_t*>(ptr) - DataRegion()) / bin_size_;
}

/** Allocates memory and returns a pointer to the memory region. */
void* SmallBlock::Alloc() {
  DCHECK(!IsFull(), "Alloc() called when IsFull()=true.");

  int free_bin_idx = FreeBinIndex();
  MarkBinUsed(free_bin_idx);

  return DataPtrForBinIndex(free_bin_idx);
}

void SmallBlock::Free(void* ptr) {
  uint32_t bin_idx = BinIndexForDataPtr(ptr);
  MarkBinFree(bin_idx);
}

size_t SmallBlock::UsedBinBitSetSize() const {
  return math::round_16b(BitSet::RequiredSize(bin_count_));
}

void SmallBlock::MarkBinFree(int index) {
  used_bin_count_--;
  UsedBinBitSet()->set(index, false);
}

void SmallBlock::MarkBinUsed(int index) {
  used_bin_count_++;
  UsedBinBitSet()->set(index, true);
}

uint8_t* SmallBlock::DataRegion() {
  return &data_[UsedBinBitSetSize()];
}

const uint8_t* SmallBlock::DataRegion() const {
  return &data_[UsedBinBitSetSize()];
}

SmallBlock::BitSet* SmallBlock::UsedBinBitSet() {
  return reinterpret_cast<BitSet*>(data_);
}

const SmallBlock::BitSet* SmallBlock::UsedBinBitSet() const {
  return reinterpret_cast<const BitSet*>(data_);
}

size_t SmallBlock::DataSize() const {
  return bin_size_;
}

SmallBlock::SmallBlock(size_t bin_size, size_t bin_count)
    : bin_size_(bin_size), bin_count_(bin_count) {
  DCHECK_GT(bin_size_, 0);
  DCHECK_GT(bin_count_, 0);
}

}  // namespace blocks
}  // namespace jsmalloc

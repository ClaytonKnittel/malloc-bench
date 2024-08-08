#include "src/jsmalloc/block.h"

#include <exception>

#include "src/jsmalloc/util/assert.h"

namespace jsmalloc {
namespace {

/**
 * Small blocks consist of uniformly sized bins.
 * Bins must be of size 16*N-8 and are picked so that we always get
 * approximately 75% utilization of a bin.
 */
constexpr SmallBlockOptions kSmallBlockSizes[kSmallBlockSizeCount] = {
  { .data_size = 12, .bin_count = 32 },  { .data_size = 28, .bin_count = 32 },
  { .data_size = 44, .bin_count = 32 },  { .data_size = 60, .bin_count = 32 },
  { .data_size = 76, .bin_count = 32 },  { .data_size = 108, .bin_count = 32 },
  { .data_size = 140, .bin_count = 26 }, { .data_size = 188, .bin_count = 20 },
  { .data_size = 252, .bin_count = 14 },
};

static_assert(kSmallBlockSizes[kSmallBlockSizeCount - 1].data_size ==
              kMaxSmallBlockDataSize);

}  // namespace

SmallBlock::FreeList& MultiSmallBlockFreeList::Find(size_t data_size) {
  DCHECK_LE(data_size, kMaxSmallBlockDataSize);
  for (int i = 0; i < kSmallBlockSizeCount; i++) {
    const SmallBlockOptions& opts = kSmallBlockSizes[i];
    if (data_size <= opts.data_size) {
      return free_lists_[i];
    }
  }
  std::terminate();
}

SmallBlock* MultiSmallBlockFreeList::Create(Mallocator* mallocator,
                                            size_t data_size) {
  DCHECK_LE(data_size, kMaxSmallBlockDataSize);
  for (auto opts : kSmallBlockSizes) {
    if (data_size <= opts.data_size) {
      return SmallBlock::New(mallocator, opts);
    }
  }
  std::terminate();
}

void MultiSmallBlockFreeList::EnsureContains(SmallBlock& block) {
  auto& free_list = MultiSmallBlockFreeList::Find(block.DataSize());
  if (!free_list.contains(block) && block.CanAlloc()) {
    free_list.insert_back(block);
  }
}

SmallBlock* SmallBlock::New(Mallocator* mallocator, SmallBlockOptions options) {
  size_t block_size = BlockSizeForBinSize(options);
  void* ptr = mallocator->malloc(block_size);
  if (ptr == nullptr) {
    return nullptr;
  }
  return new (ptr) SmallBlock(block_size, options);
}

SmallBlock::SmallBlock(size_t block_size, SmallBlockOptions options)
    : data_size_(options.data_size),
      bin_count_(options.bin_count),
      free_bins_(InitialFreeMask()) {
  DCHECK_EQ(data_size_ % 16, 12);
  DCHECK_LE(bin_count_, sizeof(free_bins_) * 8);
  DCHECK_LE(offsetof(SmallBlock, bins_) + BinSize() * bin_count_, block_size);

  header_.size = static_cast<uint32_t>(block_size);
  header_.kind = BlockKind::kSmallBlock;
  UpdateFreeBit();
  DCHECK_TRUE(IsFree());
}

/** Frees the bin associated with the provided data pointer. */
void SmallBlock::Free(void* ptr) {
  DataHeader* header = reinterpret_cast<DataHeader*>(ptr) - 1;
  int bin_index =
      (header->offset - offsetof(SmallBlock, bins_) - offsetof(Bin, data_)) /
      BinSize();
  DCHECK_LT(bin_index, bin_count_);
  DCHECK_GE(bin_index, 0);

  MarkBinFree(bin_index);
}

/** Allocates a bin and returns a pointer to the memory region. */
void* SmallBlock::Alloc() {
  DCHECK(CanAlloc(), "Alloc called when SmallBlock has no free regions.");
  int free_bin_index = std::countr_zero(free_bins_);
  Bin* bin = reinterpret_cast<Bin*>(&bins_[free_bin_index * BinSize()]);
  bin->header_.offset = offsetof(SmallBlock, bins_) +
                        free_bin_index * BinSize() + offsetof(Bin, data_);
  DCHECK_LT(free_bin_index, bin_count_);
  DCHECK_GE(free_bin_index, 0);
  MarkBinUsed(free_bin_index);
  return static_cast<void*>(bin->data_);
}

/** Whether this block is free and can be consumed. */
bool SmallBlock::IsFree() const {
  return header_.free;
}

/** Whether this block has more memory to allocate out. */
bool SmallBlock::CanAlloc() const {
  return free_bins_ != 0;
}

/** The size of this block. */
size_t SmallBlock::Size() const {
  return static_cast<size_t>(header_.size);
}

/** The size of data this block can allocate. */
size_t SmallBlock::DataSize() const {
  return data_size_;
}

uint32_t SmallBlock::InitialFreeMask() const {
  DCHECK_LE(bin_count_, 32);
  uint64_t free_mask = (static_cast<uint64_t>(1) << bin_count_) - 1;
  DCHECK_EQ(std::popcount(free_mask), bin_count_);
  return static_cast<uint32_t>(free_mask);
}

void SmallBlock::MarkBinFree(int bin_index) {
  free_bins_ |= 1 << bin_index;
  UpdateFreeBit();
}

void SmallBlock::MarkBinUsed(int bin_index) {
  free_bins_ &= ~(1 << bin_index);
  UpdateFreeBit();
}

void SmallBlock::UpdateFreeBit() {
  header_.free = std::popcount(free_bins_) == bin_count_;
}

uint32_t SmallBlock::BinSize() const {
  return offsetof(Bin, data_) + data_size_;
}

LargeBlock* LargeBlock::New(Mallocator* mallocator, size_t data_size) {
  size_t block_size = BlockSizeForDataSize(data_size);
  void* ptr = mallocator->malloc(block_size);
  if (ptr == nullptr) {
    return nullptr;
  }
  return new (ptr) LargeBlock(block_size);
}

LargeBlock::LargeBlock(size_t block_size) {
  DCHECK_EQ(block_size % 16, 0);
  header_.size = static_cast<uint32_t>(block_size);
  header_.kind = BlockKind::kLargeBlock;
  header_.free = true;
  data_header_.offset = offsetof(LargeBlock, data_);
}

/** The size of this block. */
size_t LargeBlock::Size() const {
  return header_.size;
}

bool LargeBlock::IsFree() const {
  return header_.free;
}

bool LargeBlock::CanAlloc() const {
  return header_.free;
}

void* LargeBlock::Alloc() {
  DCHECK(CanAlloc(), "Attempt to call LargeBlock::Alloc when not free");
  header_.free = false;
  return data_;
}

size_t LargeBlock::DataSize() const {
  return Size() - offsetof(LargeBlock, data_);
}

void LargeBlock::Free(void* ptr) {
  DCHECK(ptr == static_cast<void*>(data_),
         "Free called with ptr not owned by this LargeBlock");
  header_.free = true;
}

CommonBlockHeader* BlockFromDataPointer(void* ptr) {
  DataHeader* data_header = reinterpret_cast<DataHeader*>(ptr) - 1;
  return reinterpret_cast<CommonBlockHeader*>(reinterpret_cast<uint8_t*>(ptr) -
                                              data_header->offset);
}

}  // namespace jsmalloc

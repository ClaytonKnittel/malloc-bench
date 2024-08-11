#include "src/jsmalloc/blocks/small_block_allocator.h"

#include <algorithm>
#include <cstdint>

#include "src/jsmalloc/blocks/block.h"
#include "src/jsmalloc/blocks/free_block_allocator.h"
#include "src/jsmalloc/blocks/small_block.h"
#include "src/jsmalloc/util/assert.h"

namespace jsmalloc {
namespace blocks {

namespace small {

/** Returns a mask of all ones if start<=n<end, or 0 otherwise. */
constexpr uint32_t CaseMask(uint32_t n, uint32_t start, uint32_t end) {
  return static_cast<uint32_t>(0) -
         static_cast<uint32_t>((start <= n) && (n < end));
}

/**
 * The max allocable data size for each size class.
 */
constexpr uint32_t kMaxDataSizePerSizeClass[] = {
  12, 28, 44, 60, 76, 92, 108, 124, 156, 188, 220, 252
};

/**
 * Returns the size class that `data_size` belongs to.
 * The buckets correspond to kMaxDataSizePerSizeClass.
 *
 * Start is inclusive, end is inclusive.
 * ——————————————————————
 * |  class | start-end |
 * |      0 |      0-12 |
 * |      1 |     13-28 |
 * |      2 |     29-44 |
 * |      3 |     45-60 |
 * |      4 |     61-76 |
 * |      5 |     77-92 |
 * |      6 |    93-108 |
 * |      7 |   109-124 |
 * |      8 |   125-156 |
 * |      9 |   157-189 |
 * |     10 |   190-220 |
 * |     11 |   221-252 |
 * ——————————————————————
 */
static constexpr uint32_t SizeClass(uint32_t data_size) {
  data_size += 3;

  uint32_t cls = 0;
  cls |= (data_size / 16) & CaseMask(data_size, 0, 128);
  cls |= (8 + (data_size - 128) / 32) & CaseMask(data_size, 128, 256);

  // To support n >= 252, uncomment the following lines:
  //
  //   cls |= (12 + (n - 256) / 64) & CaseMask(n, 256, 512);
  //   cls |= (16 + (n - 512) / 128) & CaseMask(n, 512, 1024);

  return cls;
}

static_assert(SizeClass(SmallBlockAllocator::kMaxDataSize) + 1 ==
              SmallBlockAllocator::kSizeClasses);

/**
 * Returns the number of bins to use in a SmallBlock for a given size_class.
 *
 * This is just a heuristic that seeks to make the total size of every
 * SmallBlock about 2KB.
 */
constexpr uint32_t BinCountForSizeClass(uint32_t size_class) {
  uint32_t data_size = kMaxDataSizePerSizeClass[size_class];
  uint32_t desired_block_size = 2048;
  uint32_t bin_count =
      (desired_block_size - sizeof(SmallBlock)) / (data_size + 4);
  return std::clamp<uint32_t>(bin_count, 1, 64);
}

/**
 * Returns the data size allocable by a SmallBlock with the given size class.
 */
constexpr uint32_t DataSizeForSizeClass(uint32_t size_class) {
  return kMaxDataSizePerSizeClass[size_class];
}

static_assert(DataSizeForSizeClass(SizeClass(20)) == 28);

}  // namespace small

SmallBlockAllocator::SmallBlockAllocator(FreeBlockAllocator& allocator)
    : allocator_(allocator) {}

SmallBlock::List& SmallBlockAllocator::SmallBlockList(size_t data_size) {
  uint32_t size_class = small::SizeClass(data_size);
  return small_block_lists_[size_class];
}

SmallBlock* SmallBlockAllocator::NewSmallBlock(size_t data_size) {
  uint32_t size_class = small::SizeClass(data_size);
  return SmallBlock::New(allocator_, small::DataSizeForSizeClass(size_class),
                         small::BinCountForSizeClass(size_class));
}

void* SmallBlockAllocator::Allocate(size_t size) {
  uint32_t size_class = small::SizeClass(size);
  SmallBlock::List& allocable_block_list = small_block_lists_[size_class];

  SmallBlock* block = allocable_block_list.front();

  if (block != nullptr) {
    void* ptr = block->Alloc();
    if (block->IsFull()) {
      allocable_block_list.remove(*block);
    }
    return ptr;
  }

  block = NewSmallBlock(size);
  if (block == nullptr) {
    return nullptr;
  }

  void* ptr = block->Alloc();
  DCHECK_FALSE(block->IsFull());
  allocable_block_list.insert_front(*block);
  return ptr;
}

void SmallBlockAllocator::Free(void* ptr) {
  if (ptr == nullptr) {
    return;
  }

  BlockHeader* hdr = BlockHeader::FromDataPtr(ptr);
  DCHECK_EQ(hdr->Kind(), BlockKind::kSmall);

  auto* block = reinterpret_cast<SmallBlock*>(hdr);
  uint32_t size_class = small::SizeClass(block->DataSize());

  // If the block was full, then we would have removed it from its
  // freelist. Add it back now.
  if (block->IsFull()) {
    uint32_t size_class = small::SizeClass(block->DataSize());
    SmallBlock::List& allocable_block_list = small_block_lists_[size_class];
    allocable_block_list.insert_back(*block);
  }

  block->Free(ptr);

  // There's no more data left in the block.
  // Remove it from our datastructures and
  // return it to the free block allocator.
  if (block->IsEmpty()) {
    SmallBlock::List& allocable_block_list = small_block_lists_[size_class];
    allocable_block_list.remove(*block);
    allocator_.Free(hdr);
  }
}

}  // namespace blocks
}  // namespace jsmalloc

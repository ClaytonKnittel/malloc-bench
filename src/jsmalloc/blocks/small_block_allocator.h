#pragma once

#include <cstddef>

#include "src/jsmalloc/allocator.h"
#include "src/jsmalloc/blocks/fixed_size_free_block_allocator.h"
#include "src/jsmalloc/blocks/small_block.h"

namespace jsmalloc {
namespace blocks {

namespace small {

/** Returns the size class that `data_size` belongs to. */
static constexpr uint32_t SizeClass(uint32_t data_size) {
  if (data_size <= 8) {
    return 0;
  }
  return (data_size + 15) / 16;
}

}  // namespace small

/**
 * A malloc that only services small sizes of data.
 */
class SmallBlockAllocator {
 public:
  static constexpr size_t kBlockSize = 4096;

  explicit SmallBlockAllocator(MemRegionAllocator* allocator,
                               MemRegion* mem_region)
      : allocator_(allocator, mem_region) {}

  /** Allocates a chunk of user data from a SmallBlock. */
  void* Allocate(size_t size);

  /** Frees a chunk of user data from its SmallBlock. */
  void Free(void* ptr);

  /** Reallocates a chunk of user data from a SmallBlock. */
  void* Realloc(void* ptr, size_t size);

  /** The max allocable data size for each size class. */
  static constexpr uint32_t kMaxDataSizePerSizeClass[] = {
    8, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224, 240, 256,
  };

  /** The number of size classes that `SmallBlockAllocator` services from. */
  static constexpr size_t kSizeClasses =
      sizeof(kMaxDataSizePerSizeClass) / sizeof(kMaxDataSizePerSizeClass[0]);

  /** The maximum data size serviced by `SmallBlockAllocator`. */
  static constexpr size_t kMaxDataSize =
      kMaxDataSizePerSizeClass[kSizeClasses - 1];

 private:
  static SmallBlock* FindBlock(void* data_ptr);

  SmallBlock::List& GetSmallBlockList(size_t data_size);
  SmallBlock* NewSmallBlock(size_t data_size);

  FixedSizeFreeBlockAllocator<kBlockSize> allocator_;
  SmallBlock::List small_block_lists_[kSizeClasses];
};

}  // namespace blocks
}  // namespace jsmalloc

#pragma once

#include <cstddef>

#include "src/jsmalloc/blocks/free_block_allocator.h"
#include "src/jsmalloc/blocks/small_block.h"

namespace jsmalloc {
namespace blocks {

/**
 * A malloc that only services small sizes of data.
 */
class SmallBlockAllocator {
 public:
  explicit SmallBlockAllocator(FreeBlockAllocator& allocator);

  /** Allocates a chunk of user data from a SmallBlock. */
  void* Allocate(size_t size);

  /** Frees a chunk of user data from its SmallBlock. */
  void Free(void* ptr);

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
  SmallBlock::List& SmallBlockList(size_t data_size);
  SmallBlock* NewSmallBlock(size_t data_size);

  FreeBlockAllocator& allocator_;
  SmallBlock::List small_block_lists_[kSizeClasses];
};

}  // namespace blocks
}  // namespace jsmalloc

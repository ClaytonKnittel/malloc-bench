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

  /** The maximum data size serviced by `SmallBlockAllocator`. */
  static constexpr int kMaxDataSize = 252;

  /** The number of size classes that `SmallBlockAllocator` services from. */
  static constexpr int kSizeClasses = 12;

 private:
  SmallBlock::List& SmallBlockList(size_t data_size);
  SmallBlock* NewSmallBlock(size_t data_size);

  FreeBlockAllocator& allocator_;
  SmallBlock::List small_block_lists_[kSizeClasses];
};

}  // namespace blocks
}  // namespace jsmalloc

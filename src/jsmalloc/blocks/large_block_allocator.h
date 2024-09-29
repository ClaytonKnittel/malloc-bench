#pragma once

#include <cstddef>

#include "src/jsmalloc/blocks/free_block_allocator.h"

namespace jsmalloc {
namespace blocks {

/**
 * A malloc that only services small sizes of data.
 */
class LargeBlockAllocator {
 public:
  explicit LargeBlockAllocator(FreeBlockAllocator& allocator);

  /** Allocates a chunk of user data from a `LargeBlock`. */
  void* Allocate(size_t size, size_t alignment = 1);

  /** Frees a chunk of user data from its `LargeBlock`. */
  void Free(void* ptr);

 private:
  FreeBlockAllocator& allocator_;
};

}  // namespace blocks
}  // namespace jsmalloc

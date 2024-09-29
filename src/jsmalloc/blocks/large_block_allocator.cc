#include "src/jsmalloc/blocks/large_block_allocator.h"

#include <cstddef>

#include "src/jsmalloc/blocks/free_block.h"
#include "src/jsmalloc/blocks/free_block_allocator.h"
#include "src/jsmalloc/blocks/large_block.h"
#include "src/jsmalloc/util/twiddle.h"

namespace jsmalloc {
namespace blocks {

LargeBlockAllocator::LargeBlockAllocator(FreeBlockAllocator& allocator)
    : allocator_(allocator){};

/** Allocates a chunk of user data from a `LargeBlock`. */
void* LargeBlockAllocator::Allocate(size_t size, size_t alignment) {
  size_t required_size = size + alignment - 1;

  FreeBlock* free_block =
      allocator_.Allocate(LargeBlock::BlockSize(required_size));
  if (free_block == nullptr) {
    return nullptr;
  }
  LargeBlock* block = LargeBlock::Init(free_block, alignment);
  if (block == nullptr) {
    return nullptr;
  }
  return block->Data();
}

/** Frees a chunk of user data from its `LargeBlock`. */
void LargeBlockAllocator::Free(void* ptr) {
  if (ptr == nullptr) {
    return;
  }
  allocator_.Free(LargeBlock::FromDataPtr(ptr)->Header());
}

}  // namespace blocks
}  // namespace jsmalloc

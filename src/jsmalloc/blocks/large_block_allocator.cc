#include "src/jsmalloc/blocks/large_block_allocator.h"

#include <cstddef>

#include "src/jsmalloc/blocks/block.h"
#include "src/jsmalloc/blocks/free_block_allocator.h"
#include "src/jsmalloc/blocks/large_block.h"

namespace jsmalloc {
namespace blocks {

LargeBlockAllocator::LargeBlockAllocator(FreeBlockAllocator& allocator)
    : allocator_(allocator){};

/** Allocates a chunk of user data from a `LargeBlock`. */
void* LargeBlockAllocator::Allocate(size_t size) {
  LargeBlock* block = LargeBlock::New(allocator_, size);
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
  allocator_.Free(BlockHeader::FromDataPtr(ptr));
}

}  // namespace blocks
}  // namespace jsmalloc

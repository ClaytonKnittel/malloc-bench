#include "src/jsmalloc/blocks/large_block_allocator.h"

#include <cstddef>

#include "src/jsmalloc/blocks/block.h"
#include "src/jsmalloc/blocks/free_block.h"
#include "src/jsmalloc/blocks/free_block_allocator.h"
#include "src/jsmalloc/blocks/large_block.h"

namespace jsmalloc {
namespace blocks {

LargeBlockAllocator::LargeBlockAllocator(FreeBlockAllocator& allocator)
    : allocator_(allocator){};

/** Allocates a chunk of user data from a `LargeBlock`. */
void* LargeBlockAllocator::Allocate(size_t size) {
  FreeBlock* free_block = allocator_.Allocate(LargeBlock::BlockSize(size));
  if (free_block == nullptr) {
    return nullptr;
  }
  LargeBlock* block = LargeBlock::Init(free_block);
  if (block == nullptr) {
    return nullptr;
  }
  return block->Data();
}

void* LargeBlockAllocator::EfficientlyAllocateFromExistingBlock(size_t size) {
  size_t min_size = LargeBlock::BlockSize(size);
  auto max_size = static_cast<size_t>(min_size * 1.1);
  FreeBlock* free_block =
      allocator_.AllocateExistingBlock(min_size, max_size + 1);
  if (free_block == nullptr) {
    return nullptr;
  }
  LargeBlock* block = LargeBlock::Init(free_block);
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

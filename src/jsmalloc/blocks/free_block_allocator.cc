#include "src/jsmalloc/blocks/free_block_allocator.h"

#include "src/jsmalloc/allocator.h"
#include "src/jsmalloc/blocks/block.h"
#include "src/jsmalloc/blocks/free_block.h"
#include "src/jsmalloc/util/assert.h"

namespace jsmalloc {
namespace blocks {

FreeBlockAllocator::FreeBlockAllocator(Allocator& allocator)
    : allocator_(allocator){};

FreeBlock* FreeBlockAllocator::Allocate(size_t size) {
  DCHECK_EQ(size % 16, 0);

  for (auto& free_block : free_blocks_) {
    if (!free_block.CanResizeTo(size)) {
      continue;
    }

    free_blocks_.remove(free_block);
    FreeBlock* remainder = free_block.ResizeTo(size);
    if (remainder != nullptr) {
      free_blocks_.insert_back(*remainder);
    }
    return &free_block;
  }

  return FreeBlock::New(allocator_, size);
}

void FreeBlockAllocator::Free(BlockHeader* block) {
  free_blocks_.insert_back(*FreeBlock::Claim(block));
}

}  // namespace blocks
}  // namespace jsmalloc

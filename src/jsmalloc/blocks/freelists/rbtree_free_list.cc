#include "src/jsmalloc/blocks/freelists/rbtree_free_list.h"

#include "src/jsmalloc/blocks/free_block.h"

namespace jsmalloc {
namespace blocks {

FreeBlock* RbTreeFreeList::FindBestFit(size_t size) {
  return rbtree_.LowerBound(
      [size](const FreeBlock& block) { return block.BlockSize() >= size; });
}

void RbTreeFreeList::Remove(FreeBlock* block) {
  block->SetStorageLocation(FreeBlock::StorageLocation::kUntracked);
  rbtree_.Remove(block);
}

void RbTreeFreeList::Insert(FreeBlock* block) {
  block->SetStorageLocation(FreeBlock::StorageLocation::kRbTree);
  rbtree_.Insert(block);
}

}  // namespace blocks
}  // namespace jsmalloc

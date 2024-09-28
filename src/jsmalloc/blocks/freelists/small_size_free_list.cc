#include "src/jsmalloc/blocks/freelists/small_size_free_list.h"

#include "src/jsmalloc/blocks/free_block.h"
#include "src/jsmalloc/util/assert.h"
#include "src/jsmalloc/util/math.h"

namespace jsmalloc {
namespace blocks {

SmallSizeFreeList::SmallSizeFreeList() {
  empty_exact_size_lists_.SetRange(0, kExactSizeBins);
};

FreeBlock* SmallSizeFreeList::FindBestFit(size_t size) {
  DCHECK_LE(size, kMaxSize);
  size_t idx = empty_exact_size_lists_.FindFirstUnsetBitFrom(
      math::div_ceil(size, kBytesPerExactSizeBin));
  return exact_size_lists_[idx].front();
}

void SmallSizeFreeList::Remove(FreeBlock* block) {
  DCHECK_LE(block->BlockSize(), kMaxSize);

  block->SetStorageLocation(FreeBlock::StorageLocation::kUntracked);
  size_t idx = math::div_ceil(block->BlockSize(), kBytesPerExactSizeBin);
  FreeBlock::List::unlink(*block);
  empty_exact_size_lists_.Set(idx, exact_size_lists_[idx].empty());
}

void SmallSizeFreeList::Insert(FreeBlock* block) {
  DCHECK_LE(block->BlockSize(), kMaxSize);

  block->SetStorageLocation(FreeBlock::StorageLocation::kSmallSizeFreeList);
  size_t idx = math::div_ceil(block->BlockSize(), kBytesPerExactSizeBin);
  exact_size_lists_[idx].insert_front(*block);
  empty_exact_size_lists_.Set(idx, false);
}

}  // namespace blocks
}  // namespace jsmalloc

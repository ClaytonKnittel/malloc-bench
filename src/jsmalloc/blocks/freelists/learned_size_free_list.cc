#include "src/jsmalloc/blocks/freelists/learned_size_free_list.h"

#include "absl/hash/hash.h"

#include "src/jsmalloc/blocks/free_block.h"

namespace jsmalloc {
namespace blocks {

[[clang::noinline]] LearnedSizeFreeList::Bin* LearnedSizeFreeList::FindBin(size_t size) {
  size_t idx = absl::HashOf(size) % kBinsLen;
  return &bins_[idx];
}

void LearnedSizeFreeList::RecordAllocation(Bin* bin, size_t size) {
  if (bin->is_size_locked && size != bin->block_size) {
    return;
  }

  bool should_reset = bin->block_size != size;
  total_count_ =
      should_reset ? 1 + total_count_ - bin->count : 1 + total_count_;
  bin->count = should_reset ? 1 : bin->count + 1;
  bin->block_size = size;

  // Lock the bin to the block size if it occurs frequently enough.
  bool exceeds_thresh = bin->count * kBinsLen >= total_count_;
  bool enough_samples = total_count_ >= kMinSampleSize;
  if (exceeds_thresh && enough_samples) {
    bin->is_size_locked = true;
  }
}

FreeBlock* LearnedSizeFreeList::FindBestFit(size_t size) {
  Bin* bin = FindBin(size);
  RecordAllocation(bin, size);

  if (bin->block_size != size) {
    return nullptr;
  }
  return bin->free_blocks.front();
}

bool LearnedSizeFreeList::MaybeRemove(FreeBlock* block) {
  if (block->GetStorageLocation() !=
      FreeBlock::StorageLocation::kLearnedSizeList) {
    return false;
  }

  Bin* bin = FindBin(block->BlockSize());
  if (bin->block_size != block->BlockSize()) {
    return false;
  }

  FreeBlock::List::unlink(*block);
  size--;
  return true;
}

bool LearnedSizeFreeList::MaybeInsert(FreeBlock* block) {
  Bin* bin = FindBin(block->BlockSize());
  if (!bin->is_size_locked || bin->block_size != block->BlockSize()) {
    return false;
  }

  block->SetStorageLocation(FreeBlock::StorageLocation::kLearnedSizeList);
  bin->free_blocks.insert_front(*block);
  size++;
  return true;
}

}  // namespace blocks
}  // namespace jsmalloc

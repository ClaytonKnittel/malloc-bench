#include "src/ckmalloc/large_allocator_test_fixture.h"

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "util/absl_util.h"

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/common.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

absl::Status LargeAllocatorFixture::ValidateHeap() {
  absl::flat_hash_set<const Block*> free_blocks;
  const auto tracked_block =
      [this, &free_blocks](const FreeBlock& block) -> absl::Status {
    MappedSlab* mapped_slab =
        slab_map_->FindSlab(slab_manager_->PageIdFromPtr(&block));
    if (mapped_slab == nullptr || mapped_slab->Type() != SlabType::kBlocked) {
      return FailedTest(
          "Encountered block not within large slab in freelist: block %p of "
          "size %zu, found in slab %v",
          &block, block.Size(), mapped_slab);
    }
    BlockedSlab* slab = mapped_slab->ToBlocked();

    void* end_addr =
        PtrAdd<void>(slab_manager_->PageStartFromId(slab->EndId()), kPageSize);
    const Block* next_adjacent = block.NextAdjacentBlock();
    if (next_adjacent < &block || next_adjacent >= end_addr) {
      return FailedTest(
          "Block in freelist extends beyond the end of its slab: block %p of "
          "size %zu in %v",
          &block, block.Size(), *slab);
    }

    size_t block_offset_bytes = PtrDistance(
        static_cast<const AllocatedBlock*>(static_cast<const Block*>(&block))
            ->UserDataPtr(),
        slab_manager_->PageStartFromId(slab->StartId()));
    if (!IsAligned<uint64_t>(block_offset_bytes, kDefaultAlignment)) {
      return FailedTest(
          "Encountered unaligned block in freelist at offset %zu from heap "
          "start: %v",
          block_offset_bytes, block);
    }

    if (!static_cast<const Block*>(&block)->Free()) {
      return FailedTest("Encountered non-free block in freelist: %v", block);
    }

    if (block.Size() < Block::kMinBlockSize) {
      return FailedTest(
          "Encountered block smaller than min block size (%v): %v",
          Block::kMinBlockSize, block);
    }

    auto [it2, inserted] = free_blocks.insert(&block);
    if (!inserted) {
      return FailedTest("Detected loop in freelist at block %v", block);
    }

    return absl::OkStatus();
  };

  // Iterate over the large freelist.
  const FreeBlock* prev_block = nullptr;
  for (const FreeBlock& block : Freelist().large_blocks_tree_) {
    RETURN_IF_ERROR(tracked_block(block));

    if (prev_block != nullptr && prev_block->Size() > block.Size()) {
      return FailedTest("Freelist not sorted by block size: %v > %v",
                        *prev_block, block);
    }
    prev_block = &block;
  }

  // Iterate over all exact-size bins.
  for (size_t idx = 0; idx < Freelist::kNumExactSizeBins; idx++) {
    const uint64_t expected_block_size =
        Block::kMinTrackedSize + kDefaultAlignment * idx;

    for (const TrackedBlock& block : Freelist().exact_size_bins_[idx]) {
      RETURN_IF_ERROR(tracked_block(block));

      if (block.Size() != expected_block_size) {
        return FailedTest(
            "Found block with unexpected size in freelist idx %v: expected "
            "size=%v, found size=%v",
            idx, expected_block_size, block.Size());
      }
    }
  }

  // Iterate over the heap.
  size_t n_free_blocks = 0;
  PageId page = PageId::Zero();
  while (true) {
    MappedSlab* mapped_slab = SlabMap().FindSlab(page);
    if (mapped_slab == nullptr) {
      break;
    }

    if (mapped_slab->Type() != SlabType::kBlocked) {
      page += mapped_slab->Pages();
      continue;
    }

    BlockedSlab* slab = mapped_slab->ToBlocked();

    void* const slab_start = slab_manager_->PageStartFromId(slab->StartId());
    void* const slab_end =
        PtrAdd<void>(slab_manager_->PageStartFromId(slab->EndId()), kPageSize);
    Block* block = slab_manager_->FirstBlockInBlockedSlab(slab);
    Block* prev_block = nullptr;
    uint64_t allocated_bytes = 0;
    while (!block->IsPhonyHeader()) {
      if (block < slab_start || block->NextAdjacentBlock() >= slab_end) {
        return FailedTest(
            "Encountered block outside the range of the heap while iterating "
            "over heap: block at %p, heap ranges from %p to %p",
            &block, slab_start, slab_end);
      }

      size_t block_offset_bytes = PtrDistance(
          static_cast<AllocatedBlock*>(block)->UserDataPtr(), slab_start);
      if (!IsAligned<uint64_t>(block_offset_bytes, kDefaultAlignment)) {
        return FailedTest(
            "Encountered unaligned block while iterating heap at "
            "offset %zu from heap start: %v",
            block_offset_bytes, *block);
      }

      if (block->Free()) {
        bool in_freelist = free_blocks.contains(block);
        if (block->IsUntracked() && in_freelist) {
          return FailedTest("Encountered untracked block in the freelist: %v",
                            *block);
        }
        if (!block->IsUntracked()) {
          if (!in_freelist) {
            return FailedTest(
                "Encountered free block which was not in freelist: %v", *block);
          }
          n_free_blocks++;
        }

        if (prev_block != nullptr && prev_block->Free()) {
          return FailedTest("Encountered two free blocks in a row: %v and %v",
                            *prev_block, *block);
        }
      } else {
        if (block->Size() < Block::kMinTrackedSize) {
          return FailedTest(
              "Encountered allocated block less than min tracked size (%v), "
              "which should not be possible: %v",
              Block::kMinTrackedSize, *block);
        }

        allocated_bytes += block->Size();
      }

      if (prev_block != nullptr && prev_block->Free()) {
        if (!block->PrevFree()) {
          return FailedTest(
              "Prev-free bit not set in block after free block: %v followed by "
              "%v",
              *prev_block, *block);
        }
        if (block->PrevSize() != prev_block->Size()) {
          return FailedTest(
              "Prev-size incorrect for block after free block: %v followed by "
              "%v",
              *prev_block, *block);
        }
      } else if (block->PrevFree()) {
        if (prev_block == nullptr) {
          return FailedTest(
              "Prev free not set correctly in block %v at beginning of slab",
              *block);
        }
        return FailedTest("Prev free not set correctly in block %v, prev %v",
                          *block, *prev_block);
      }

      if (block->Size() > PtrDistance(slab_end, block)) {
        return FailedTest(
            "Encountered block with size larger than remainder of heap: %v, "
            "heap has %zu bytes left",
            *block, PtrDistance(slab_end, block));
      }

      prev_block = block;
      block = block->NextAdjacentBlock();
    }

    Block* const phony_header =
        PtrSub<Block>(slab_end, Block::kMetadataOverhead);
    if (block != phony_header) {
      return FailedTest(
          "Ended heap iteration on block not at end of heap: %p, end of heap "
          "is %p",
          block, phony_header);
    }

    if (prev_block != nullptr && block->PrevFree() != prev_block->Free()) {
      return FailedTest(
          "Prev-free bit of phony header is incorrect: %v, prev %v", *block,
          *prev_block);
    }

    if (allocated_bytes != slab->AllocatedBytes()) {
      return FailedTest(
          "Large slab allocated byte count is incorrect for %v, expected "
          "%" PRIu64 " allocated bytes",
          *slab, allocated_bytes);
    }

    page += mapped_slab->Pages();
  }

  if (n_free_blocks != free_blocks.size()) {
    return FailedTest(
        "Encountered %zu free blocks when iterating over the heap, but %zu "
        "free blocks in the freelist",
        n_free_blocks, free_blocks.size());
  }

  return absl::OkStatus();
}

/* static */
absl::Status LargeAllocatorFixture::ValidateEmpty() {
  // No work needed to be done. Slab manager will check that all slabs are free,
  // and any block in the freelist will be invalid because it won't be within a
  // blocked slab.
  return absl::OkStatus();
}

}  // namespace ckmalloc

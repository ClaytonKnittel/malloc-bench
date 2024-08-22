#include "src/ckmalloc/testlib.h"

#include <new>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"

#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

namespace {

std::vector<std::pair<void*, std::align_val_t>> allocs;

DetachedMetadataAlloc default_detached_allocator;

}  // namespace

TestMetadataAllocInterface* TestGlobalMetadataAlloc::allocator_ =
    &default_detached_allocator;

Slab* DetachedMetadataAlloc::SlabAlloc() {
#ifdef __cpp_aligned_new
  return reinterpret_cast<Slab*>(::operator new(
      sizeof(Slab), static_cast<std::align_val_t>(alignof(Slab))));
#else
  return reinterpret_cast<Slab*>(::operator new(sizeof(Slab)));
#endif
}

void DetachedMetadataAlloc::SlabFree(MappedSlab* slab) {
#ifdef __cpp_aligned_new
  ::operator delete(slab, static_cast<std::align_val_t>(alignof(Slab)));
#else
  ::operator delete(slab);
#endif
}

void* DetachedMetadataAlloc::Alloc(size_t size, size_t alignment) {
  auto align_val = static_cast<std::align_val_t>(alignment);
#ifdef __cpp_aligned_new
  void* ptr = ::operator new(size, align_val);
#else
  void* ptr = ::operator new(size);
#endif

  allocs.emplace_back(ptr, align_val);
  return ptr;
}

void DetachedMetadataAlloc::ClearAllAllocs() {
  for (auto [ptr, align_val] : allocs) {
#ifdef __cpp_aligned_new
    ::operator delete(ptr, align_val);
#else
    ::operator delete(ptr);
#endif
  }
  allocs.clear();
}

Slab* TestGlobalMetadataAlloc::SlabAlloc() {
  return allocator_->SlabAlloc();
}

void TestGlobalMetadataAlloc::SlabFree(MappedSlab* slab) {
  allocator_->SlabFree(slab);
}

void* TestGlobalMetadataAlloc::Alloc(size_t size, size_t alignment) {
  return allocator_->Alloc(size, alignment);
}

void TestGlobalMetadataAlloc::ClearAllAllocs() {
  allocator_->ClearAllAllocs();
}

/* static */
void TestGlobalMetadataAlloc::OverrideAllocator(
    TestMetadataAllocInterface* allocator) {
  CK_ASSERT_EQ(allocator_, &default_detached_allocator);
  allocator_ = allocator;
}

/* static */
void TestGlobalMetadataAlloc::ClearAllocatorOverride() {
  allocator_ = &default_detached_allocator;
}

absl::Status ValidateLargeSlabs(const std::vector<LargeSlabInfo>& slabs,
                                const Freelist& freelist) {
  absl::flat_hash_set<const Block*> free_blocks;
  // Iterate over the freelist.
  for (const FreeBlock& block : freelist.free_blocks_) {
    auto it = absl::c_find_if(slabs, [&block](const LargeSlabInfo& slab_info) {
      return &block >= slab_info.start &&
             block.NextAdjacentBlock() <= slab_info.end;
    });
    if (it == slabs.end()) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Encountered block outside the range of the heap in "
                          "freelist: block %p of size %zu",
                          &block, block.Size()));
    }

    LargeSlabInfo slab_info = *it;

    size_t block_offset_bytes = PtrDistance(&block, slab_info.start);
    if (!IsAligned<uint64_t>(block_offset_bytes, kDefaultAlignment)) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Encountered unaligned block in freelist at offset "
                          "%zu from heap start: %v",
                          block_offset_bytes, block));
    }

    if (!static_cast<const Block*>(&block)->Free()) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Encountered non-free block in freelist: %v", block));
    }

    auto [it2, inserted] = free_blocks.insert(&block);
    if (!inserted) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Detected loop in freelist at block %v", block));
    }
  }

  // Iterate over the heap.
  size_t n_free_blocks = 0;
  for (const LargeSlabInfo& slab_info : slabs) {
    Block* block = reinterpret_cast<Block*>(slab_info.start);
    Block* prev_block = nullptr;
    uint64_t allocated_bytes = 0;
    while (block->Size() != 0) {
      if (block < slab_info.start ||
          block->NextAdjacentBlock() >= slab_info.end) {
        return absl::FailedPreconditionError(absl::StrFormat(
            "Encountered block outside the range of the heap while iterating "
            "over heap: block at %p, heap ranges from %p to %p",
            &block, slab_info.start, slab_info.end));
      }

      size_t block_offset_bytes = PtrDistance(block, slab_info.start);
      if (!IsAligned<uint64_t>(block_offset_bytes, kDefaultAlignment)) {
        return absl::FailedPreconditionError(absl::StrFormat(
            "Encountered unaligned block while iterating heap at "
            "offset %zu from heap start: %v",
            block_offset_bytes, *block));
      }

      if (block->Free()) {
        bool in_freelist = free_blocks.contains(block);
        if (block->IsUntracked() && in_freelist) {
          return absl::FailedPreconditionError(absl::StrFormat(
              "Encountered untracked block in the freelist: %v", *block));
        }
        if (!block->IsUntracked()) {
          if (!in_freelist) {
            return absl::FailedPreconditionError(absl::StrFormat(
                "Encountered free block which was not in freelist: %v",
                *block));
          }
          n_free_blocks++;
        }

        if (prev_block != nullptr && prev_block->Free()) {
          return absl::FailedPreconditionError(
              absl::StrFormat("Encountered two free blocks in a row: %v and %v",
                              *prev_block, *block));
        }
      } else {
        if (block->Size() < Block::kMinLargeSize) {
          return absl::FailedPreconditionError(
              absl::StrFormat("Encountered small-sized allocated block, which "
                              "should not be possible: %v",
                              *block));
        }

        allocated_bytes += block->Size();
      }

      if (prev_block != nullptr && prev_block->Free()) {
        if (!block->PrevFree()) {
          return absl::FailedPreconditionError(absl::StrFormat(
              "Prev-free bit not set in block after free block: "
              "%v followed by %v",
              *prev_block, *block));
        }
        if (block->PrevSize() != prev_block->Size()) {
          return absl::FailedPreconditionError(
              absl::StrFormat("Prev-size incorrect for block after free block: "
                              "%v followed by %v",
                              *prev_block, *block));
        }
      } else if (block->PrevFree()) {
        if (prev_block == nullptr) {
          return absl::FailedPreconditionError(absl::StrFormat(
              "Prev free not set correctly in block %v at beginning of slab",
              *block));
        }
        return absl::FailedPreconditionError(
            absl::StrFormat("Prev free not set correctly in block %v, prev %v",
                            *block, *prev_block));
      }

      if (block->Size() > PtrDistance(slab_info.end, block)) {
        return absl::FailedPreconditionError(
            absl::StrFormat("Encountered block with size larger than remainder "
                            "of heap: %v, heap has %zu bytes left",
                            *block, PtrDistance(slab_info.end, block)));
      }

      prev_block = block;
      block = block->NextAdjacentBlock();
    }

    if (block != reinterpret_cast<Block*>(
                     reinterpret_cast<uint64_t*>(slab_info.end) - 1)) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Ended heap iteration on block not at end of heap: %p, "
          "end of heap is %p",
          block, reinterpret_cast<uint64_t*>(slab_info.end) - 1));
    }

    if (prev_block != nullptr && block->PrevFree() != prev_block->Free()) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Prev-free bit of phony header is incorrect: %v, prev %v", *block,
          *prev_block));
    }

    if (slab_info.slab != nullptr &&
        allocated_bytes != slab_info.slab->AllocatedBytes()) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Large slab allocated byte count is incorrect for "
                          "%v, expected %" PRIu64 " allocated bytes",
                          *slab_info.slab, allocated_bytes));
    }
  }

  if (n_free_blocks != free_blocks.size()) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Encountered %zu free blocks when iterating over the "
                        "heap, but %zu free blocks in the freelist",
                        n_free_blocks, free_blocks.size()));
  }

  return absl::OkStatus();
}

}  // namespace ckmalloc

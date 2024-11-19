#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/common.h"
#include "src/ckmalloc/freelist.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/size_class.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_manager.h"
#include "src/ckmalloc/slab_map.h"
#include "src/ckmalloc/slice.h"
#include "src/ckmalloc/small_freelist.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
class SmallAllocatorImpl {
  friend class SmallAllocatorFixture;
  friend class GlobalState;

  using SmallFreelist = SmallFreelistImpl<SlabMap, SlabManager>;

 public:
  explicit SmallAllocatorImpl(SlabMap* slab_map, SlabManager* slab_manager,
                              Freelist* freelist)
      : freelists_(AllSizeClassesArray<SmallFreelist>(
            [slab_map, slab_manager](SizeClass size_class) {
              return SmallFreelist(size_class, slab_map, slab_manager);
            })),
        slab_map_(slab_map),
        slab_manager_(slab_manager),
        freelist_(freelist) {}

  Void* AllocSmall(size_t user_size,
                   std::optional<size_t> alignment = std::nullopt);

  // Reallocates a small slice to another small slice size.
  Void* ReallocSmall(SmallSlab* slab, Void* ptr, size_t user_size);

  void FreeSmall(SmallSlab* slab, Void* ptr);

 private:
  SmallFreelist& SmallFreelistForSize(SizeClass size_class);

  std::array<SmallFreelist, SizeClass::kNumSizeClasses> freelists_;

  SlabMap* const slab_map_;
  SlabManager* const slab_manager_;
  Freelist* const freelist_;
};

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
Void* SmallAllocatorImpl<SlabMap, SlabManager>::AllocSmall(
    size_t user_size, std::optional<size_t> alignment) {
  SizeClass size_class = SizeClass::FromUserDataSize(user_size, alignment);
  SmallFreelist& freelist = SmallFreelistForSize(size_class);

  auto slice_from_freelist = freelist.FindSliceInFreelist();
  if (slice_from_freelist.has_value()) {
    return slice_from_freelist.value()->UserDataPtr();
  }

  // TODO: Test this in main allocator test.
  uint64_t block_size = Block::BlockSizeForUserSize(user_size);
  if (block_size >= Block::kMinTrackedSize) {
    TrackedBlock* block =
        alignment.has_value()
            ? freelist_->FindFreeLazyAligned(block_size, alignment.value())
            : freelist_->FindFreeLazy(block_size);
    if (block != nullptr) {
      // TODO: unify this logic in freelist.
      BlockedSlab* slab =
          slab_map_->FindSlab(PageId::FromPtr(block))->ToBlocked();
      AllocatedBlock* allocated_block;
      if (alignment.has_value()) {
        auto [prev_free, allocated, next_free] =
            freelist_->SplitAligned(block, block_size, alignment.value());
        allocated_block = allocated;
      } else {
        auto [allocated, free] = freelist_->Split(block, block_size);
        allocated_block = allocated;
      }
      CK_ASSERT_EQ(allocated_block->Size(), block_size);
      slab->AddAllocation(block_size);
      return allocated_block->UserDataPtr();
    }
  }

  auto slice_from_new_slab = freelist.TakeSliceFromNewSlab();
  if (slice_from_new_slab.has_value()) {
    return slice_from_new_slab.value()->UserDataPtr();
  }

  return nullptr;
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
Void* SmallAllocatorImpl<SlabMap, SlabManager>::ReallocSmall(SmallSlab* slab,
                                                             Void* ptr,
                                                             size_t user_size) {
  CK_ASSERT_NE(user_size, 0);
  CK_ASSERT_LE(user_size, kMaxSmallSize);
  if (ptr == nullptr) {
    return AllocSmall(user_size);
  }

  CK_ASSERT_EQ(slab_map_->FindSlab(PageId::FromPtr(ptr))->ToSmall(), slab);
  SizeClass size_class = SizeClass::FromUserDataSize(user_size);
  SizeClass cur_size_class = slab->SizeClass();
  if (cur_size_class == size_class) {
    return ptr;
  }

  Void* new_ptr = AllocSmall(user_size);
  if (new_ptr != nullptr) {
    std::memcpy(new_ptr, ptr, std::min(user_size, cur_size_class.SliceSize()));
    FreeSmall(slab, ptr);
  }
  return new_ptr;
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
void SmallAllocatorImpl<SlabMap, SlabManager>::FreeSmall(SmallSlab* slab,
                                                         Void* ptr) {
  SmallFreelistForSize(slab->SizeClass())
      .ReturnSlice(slab, AllocatedSlice::FromUserDataPtr(ptr));
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
SmallFreelistImpl<SlabMap, SlabManager>&
SmallAllocatorImpl<SlabMap, SlabManager>::SmallFreelistForSize(
    SizeClass size_class) {
  return freelists_[size_class.Ordinal()];
}

using SmallAllocator = SmallAllocatorImpl<SlabMap, SlabManager>;

}  // namespace ckmalloc

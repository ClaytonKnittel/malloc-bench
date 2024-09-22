#pragma once

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
#include "src/ckmalloc/util.h"

namespace ckmalloc {

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
class SmallAllocatorImpl {
  friend class SmallAllocatorFixture;
  friend class GlobalState;

 public:
  explicit SmallAllocatorImpl(SlabMap* slab_map, SlabManager* slab_manager,
                              Freelist* freelist)
      : slab_map_(slab_map), slab_manager_(slab_manager), freelist_(freelist) {}

  Void* AllocSmall(size_t user_size);

  // Reallocates a small slice to another small slice size.
  Void* ReallocSmall(SmallSlab* slab, Void* ptr, size_t user_size);

  void FreeSmall(SmallSlab* slab, Void* ptr);

 private:
  // Returns a slice from the freelist if there is one, or `std::nullopt` if the
  // freelist is empty.
  std::optional<AllocatedSlice*> FindSliceInFreelist(SizeClass size_class);

  // Allocates a single slice from this small blocks slab, which must not be
  // full.
  // TODO: return multiple once we have a cache?
  AllocatedSlice* TakeSlice(SmallSlab* slab);

  // Allocates a new slab of the given size class, inserting it into the
  // freelist and returning a slice from it.
  std::optional<AllocatedSlice*> TakeSliceFromNewSlab(SizeClass size_class);

  // Returns a slice to the small slab, allowing it to be reallocated.
  void ReturnSlice(SmallSlab* slab, AllocatedSlice* slice);

  PageId& FreelistHead(SizeClass size_class);

  void AddToFreelist(SmallSlab* slab);

  void RemoveFromFreelist(SmallSlab* slab);

  PageId freelists_[SizeClass::kNumSizeClasses] = {};

  SlabMap* const slab_map_;
  SlabManager* const slab_manager_;
  Freelist* const freelist_;
};

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
Void* SmallAllocatorImpl<SlabMap, SlabManager>::AllocSmall(size_t user_size) {
  SizeClass size_class = SizeClass::FromUserDataSize(user_size);

  auto slice_from_freelist = FindSliceInFreelist(size_class);
  if (slice_from_freelist.has_value()) {
    return slice_from_freelist.value()->UserDataPtr();
  }

  // TODO: Test this in main allocator test.
  uint64_t block_size = Block::BlockSizeForUserSize(user_size);
  if (block_size >= Block::kMinTrackedSize) {
    TrackedBlock* block = freelist_->FindFreeExact(block_size);
    if (block != nullptr) {
      BlockedSlab* slab =
          slab_map_->FindSlab(PageId::FromPtr(block))->ToBlocked();
      slab->AddAllocation(Block::BlockSizeForUserSize(user_size));
      CK_ASSERT_EQ(block->Size(), block_size);
      auto [allocated, free] = freelist_->Split(block, block_size);
      CK_ASSERT_EQ(free, nullptr);
      return allocated->UserDataPtr();
    }
  }

  auto slice_from_new_slab = TakeSliceFromNewSlab(size_class);
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
  CK_ASSERT_EQ(slab_map_->FindSlab(PageId::FromPtr(ptr))->ToSmall(), slab);
  if (slab->Full()) {
    AddToFreelist(slab);
  }

  ReturnSlice(slab, AllocatedSlice::FromUserDataPtr(ptr));
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
std::optional<AllocatedSlice*>
SmallAllocatorImpl<SlabMap, SlabManager>::FindSliceInFreelist(
    SizeClass size_class) {
  PageId first_in_freelist = FreelistHead(size_class);
  if (first_in_freelist == PageId::Nil()) {
    return std::nullopt;
  }

  return TakeSlice(slab_map_->FindSlab(first_in_freelist)->ToSmall());
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
AllocatedSlice* SmallAllocatorImpl<SlabMap, SlabManager>::TakeSlice(
    SmallSlab* slab) {
  CK_ASSERT_FALSE(slab->Full());
  AllocatedSlice* slice = slab->PopSlice(slab->StartId().PageStart());

  if (slab->Full()) {
    RemoveFromFreelist(slab);
  }
  return slice;
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
std::optional<AllocatedSlice*>
SmallAllocatorImpl<SlabMap, SlabManager>::TakeSliceFromNewSlab(
    SizeClass size_class) {
  using AllocRes = std::pair<PageId, SmallSlab*>;
  DEFINE_OR_RETURN_OPT(AllocRes, result,
                       slab_manager_->template Alloc<SmallSlab>(1, size_class));
  auto [page_id, slab] = result;

  CK_ASSERT_EQ(FreelistHead(size_class), PageId::Nil());
  AddToFreelist(slab);
  return TakeSlice(slab);
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
void SmallAllocatorImpl<SlabMap, SlabManager>::ReturnSlice(
    SmallSlab* slab, AllocatedSlice* slice) {
  void* slab_start = slab->StartId().PageStart();
  CK_ASSERT_GE(slice, slab_start);
  CK_ASSERT_LE(
      slice, PtrAdd<AllocatedSlice>(slab_start,
                                    kPageSize - slab->SizeClass().SliceSize()));

  slab->PushSlice(slab->StartId().PageStart(), slice);
  if (slab->Empty()) {
    RemoveFromFreelist(slab);
    slab_manager_->Free(slab);
  }
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
PageId& SmallAllocatorImpl<SlabMap, SlabManager>::FreelistHead(
    SizeClass size_class) {
  return freelists_[size_class.Ordinal()];
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
void SmallAllocatorImpl<SlabMap, SlabManager>::AddToFreelist(SmallSlab* slab) {
  PageId page_id = slab->StartId();
  PageId& freelist = FreelistHead(slab->SizeClass());
  slab->SetNextFree(freelist);
  slab->SetPrevFree(PageId::Nil());

  if (freelist != PageId::Nil()) {
    SmallSlab* prev_head = slab_map_->FindSlab(freelist)->ToSmall();
    prev_head->SetPrevFree(slab->StartId());
  }
  freelist = page_id;
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
void SmallAllocatorImpl<SlabMap, SlabManager>::RemoveFromFreelist(
    SmallSlab* slab) {
  PageId prev_id = slab->PrevFree();
  PageId next_id = slab->NextFree();
  if (prev_id != PageId::Nil()) {
    slab_map_->FindSlab(prev_id)->ToSmall()->SetNextFree(next_id);
  } else {
    FreelistHead(slab->SizeClass()) = next_id;
  }
  if (next_id != PageId::Nil()) {
    slab_map_->FindSlab(next_id)->ToSmall()->SetPrevFree(prev_id);
  }
}

using SmallAllocator = SmallAllocatorImpl<SlabMap, SlabManager>;

}  // namespace ckmalloc

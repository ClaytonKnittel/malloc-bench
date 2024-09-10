#pragma once

#include <cstring>
#include <optional>

#include "src/ckmalloc/common.h"
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

 public:
  explicit SmallAllocatorImpl(SlabMap* slab_map, SlabManager* slab_manager)
      : slab_map_(slab_map), slab_manager_(slab_manager) {}

  AllocatedSlice* AllocSlice(size_t user_size);

  // Reallocates a small slice to another small slice size.
  AllocatedSlice* ReallocSlice(SmallSlab* slab, AllocatedSlice* slice,
                               size_t user_size);

  void FreeSlice(SmallSlab* slab, AllocatedSlice* slice);

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

  static_assert(SizeClass::kNumSizeClasses == 9);
  PageId freelists_[SizeClass::kNumSizeClasses] = {
    PageId::Nil(), PageId::Nil(), PageId::Nil(), PageId::Nil(), PageId::Nil(),
    PageId::Nil(), PageId::Nil(), PageId::Nil(), PageId::Nil(),
  };

  SlabMap* const slab_map_;
  SlabManager* const slab_manager_;
};

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
AllocatedSlice* SmallAllocatorImpl<SlabMap, SlabManager>::AllocSlice(
    size_t user_size) {
  SizeClass size_class = SizeClass::FromUserDataSize(user_size);

  auto slice_from_freelist = FindSliceInFreelist(size_class);
  if (slice_from_freelist.has_value()) {
    return slice_from_freelist.value();
  }

  auto slice_from_new_slab = TakeSliceFromNewSlab(size_class);
  if (slice_from_new_slab.has_value()) {
    return slice_from_new_slab.value();
  }

  return nullptr;
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
AllocatedSlice* SmallAllocatorImpl<SlabMap, SlabManager>::ReallocSlice(
    SmallSlab* slab, AllocatedSlice* slice, size_t user_size) {
  CK_ASSERT_NE(user_size, 0);
  CK_ASSERT_LE(user_size, kMaxSmallSize);
  if (slice == nullptr) {
    return AllocSlice(user_size);
  }

  CK_ASSERT_EQ(
      slab_map_->FindSlab(slab_manager_->PageIdFromPtr(slice))->ToSmall(),
      slab);
  SizeClass size_class = SizeClass::FromUserDataSize(user_size);
  SizeClass cur_size_class = slab->SizeClass();
  if (cur_size_class == size_class) {
    return slice;
  }

  AllocatedSlice* new_slice = AllocSlice(user_size);
  if (new_slice != nullptr) {
    std::memcpy(new_slice->UserDataPtr(), slice->UserDataPtr(),
                std::min(size_class.SliceSize(), cur_size_class.SliceSize()));
    FreeSlice(slab, slice);
  }
  return new_slice;
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
void SmallAllocatorImpl<SlabMap, SlabManager>::FreeSlice(
    SmallSlab* slab, AllocatedSlice* slice) {
  CK_ASSERT_EQ(
      slab_map_->FindSlab(slab_manager_->PageIdFromPtr(slice))->ToSmall(),
      slab);
  if (slab->Full()) {
    AddToFreelist(slab);
  }

  ReturnSlice(slab, slice);
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
  AllocatedSlice* slice =
      slab->PopSlice(slab_manager_->PageStartFromId(slab->StartId()));

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
  void* slab_start = slab_manager_->PageStartFromId(slab->StartId());
  CK_ASSERT_GE(slice, slab_start);
  CK_ASSERT_LE(
      slice, PtrAdd<AllocatedSlice>(slab_start,
                                    kPageSize - slab->SizeClass().SliceSize()));

  slab->PushSlice(slab_manager_->PageStartFromId(slab->StartId()), slice);
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

#pragma once

#include <optional>

#include "absl/synchronization/mutex.h"

#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/size_class.h"
#include "src/ckmalloc/slab_manager.h"
#include "src/ckmalloc/slab_map.h"
#include "src/ckmalloc/slice.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
class SmallFreelistImpl {
 public:
  SmallFreelistImpl(SizeClass size_class, SlabMap* slab_map,
                    SlabManager* slab_manager)
      : size_class_(size_class),
        slab_map_(slab_map),
        slab_manager_(slab_manager) {}

  SizeClass SizeClass() const {
    return size_class_;
  }

  PageId AvailableSlabsHead() const;

  // Returns a slice from the freelist if there is one, or `std::nullopt` if the
  // freelist is empty.
  std::optional<AllocatedSlice*> FindSliceInFreelist();

  // Allocates a new slab of the given size class, inserting it into the
  // freelist and returning a slice from it.
  std::optional<AllocatedSlice*> FindSlice();

  // Returns a slice to the small slab, allowing it to be reallocated.
  void ReturnSlice(SmallSlab* slab, AllocatedSlice* slice);

 private:
  std::optional<AllocatedSlice*> MaybeTakeSliceFromFreelist()
      CK_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  std::optional<AllocatedSlice*> TakeSliceFromNewSlab()
      CK_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Allocates a single slice from this small blocks slab, which must not be
  // full.
  // TODO: return multiple once we have a cache?
  AllocatedSlice* TakeSlice(SmallSlab* slab)
      CK_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void AddToFreelist(SmallSlab* slab) CK_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void RemoveFromFreelist(SmallSlab* slab) CK_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable absl::Mutex mutex_;

  PageId available_slabs_head_ CK_GUARDED_BY(mutex_);

  const class SizeClass size_class_;

  SlabMap* const slab_map_;
  SlabManager* const slab_manager_;
};

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
PageId SmallFreelistImpl<SlabMap, SlabManager>::AvailableSlabsHead() const {
  absl::MutexLock lock(&mutex_);
  return available_slabs_head_;
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
std::optional<AllocatedSlice*>
SmallFreelistImpl<SlabMap, SlabManager>::FindSliceInFreelist() {
  absl::MutexLock lock(&mutex_);
  return MaybeTakeSliceFromFreelist();
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
std::optional<AllocatedSlice*>
SmallFreelistImpl<SlabMap, SlabManager>::FindSlice() {
  absl::MutexLock lock(&mutex_);
  return OptionalOrElse(
      MaybeTakeSliceFromFreelist(),
      [this]() CK_NO_THREAD_SAFETY_ANALYSIS { return TakeSliceFromNewSlab(); });
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
void SmallFreelistImpl<SlabMap, SlabManager>::ReturnSlice(
    SmallSlab* slab, AllocatedSlice* slice) {
  CK_ASSERT_EQ(
      slab_map_->FindSlab(PageId::FromPtr(slice->UserDataPtr()))->ToSmall(),
      slab);

  void* slab_start = slab->StartId().PageStart();
  CK_ASSERT_GE(slice, slab_start);
  CK_ASSERT_LE(slice, PtrAdd<AllocatedSlice>(
                          slab_start, slab->SizeClass().Pages() * kPageSize -
                                          slab->SizeClass().SliceSize()));

  absl::MutexLock lock(&mutex_);
  if (slab->Full()) {
    AddToFreelist(slab);
  }

  slab->PushSlice(slab->StartId().PageStart(), slice);
  if (slab->Empty()) {
    RemoveFromFreelist(slab);
    slab_manager_->Free(slab);
  }
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
std::optional<AllocatedSlice*>
SmallFreelistImpl<SlabMap, SlabManager>::MaybeTakeSliceFromFreelist() {
  if (available_slabs_head_ == PageId::Nil()) {
    return std::nullopt;
  }
  return TakeSlice(slab_map_->FindSlab(available_slabs_head_)->ToSmall());
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
std::optional<AllocatedSlice*>
SmallFreelistImpl<SlabMap, SlabManager>::TakeSliceFromNewSlab() {
  using AllocRes = std::pair<PageId, SmallSlab*>;
  DEFINE_OR_RETURN_OPT(AllocRes, result,
                       slab_manager_->template Alloc<SmallSlab>(
                           size_class_.Pages(), size_class_));
  auto [page_id, slab] = result;

  CK_ASSERT_EQ(available_slabs_head_, PageId::Nil());
  AddToFreelist(slab);
  return TakeSlice(slab);
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
AllocatedSlice* SmallFreelistImpl<SlabMap, SlabManager>::TakeSlice(
    SmallSlab* slab) {
  CK_ASSERT_FALSE(slab->Full());
  AllocatedSlice* slice = slab->PopSlice(slab->StartId().PageStart());

  if (slab->Full()) {
    RemoveFromFreelist(slab);
  }
  return slice;
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
void SmallFreelistImpl<SlabMap, SlabManager>::AddToFreelist(SmallSlab* slab) {
  PageId page_id = slab->StartId();
  slab->SetNextFree(available_slabs_head_);
  slab->SetPrevFree(PageId::Nil());

  if (available_slabs_head_ != PageId::Nil()) {
    SmallSlab* prev_head =
        slab_map_->FindSlab(available_slabs_head_)->ToSmall();
    prev_head->SetPrevFree(slab->StartId());
  }
  available_slabs_head_ = page_id;
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
void SmallFreelistImpl<SlabMap, SlabManager>::RemoveFromFreelist(
    SmallSlab* slab) {
  PageId prev_id = slab->PrevFree();
  PageId next_id = slab->NextFree();
  if (prev_id != PageId::Nil()) {
    slab_map_->FindSlab(prev_id)->ToSmall()->SetNextFree(next_id);
  } else {
    available_slabs_head_ = next_id;
  }
  if (next_id != PageId::Nil()) {
    slab_map_->FindSlab(next_id)->ToSmall()->SetPrevFree(prev_id);
  }
}

using SmallFreelist = SmallFreelistImpl<SlabMap, SlabManager>;

}  // namespace ckmalloc
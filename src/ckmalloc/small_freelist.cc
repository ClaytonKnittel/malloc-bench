#include "src/ckmalloc/small_freelist.h"

#include <optional>

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/size_class.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slice.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

AllocatedSlice* SmallFreelist::AllocSlice(size_t user_size) {
  SizeClass size_class = SizeClass::FromUserDataSize(user_size);

  auto slice_from_freelist = FindSliceInFreelist(size_class);
  if (slice_from_freelist.has_value()) {
    return slice_from_freelist.value();
  }

  return nullptr;
}

void SmallFreelist::FreeSlice(AllocatedSlice* slice) {
  SmallSlab* slab =
      slab_map_->FindSlab(slab_manager_->PageIdFromPtr(slice))->ToSmall();
  if (slab->Full()) {
    AddToFreelist(slab);
  }

  ReturnSlice(slab, slice);
}

std::optional<AllocatedSlice*> SmallFreelist::FindSliceInFreelist(
    SizeClass size_class) {
  PageId first_in_freelist = FreelistHead(size_class);
  if (first_in_freelist == PageId::Nil()) {
    return std::nullopt;
  }

  return TakeSlice(slab_map_->FindSlab(first_in_freelist)->ToSmall());
}

AllocatedSlice* SmallFreelist::TakeSlice(SmallSlab* slab) {
  CK_ASSERT_FALSE(slab->Full());
  AllocatedSlice* slice =
      slab->PopSlice(slab_manager_->PageStartFromId(slab->StartId()));

  if (slab->Full()) {
    RemoveFromFreelist(slab);
  }
  return slice;
}

std::optional<AllocatedSlice*> SmallFreelist::TakeSliceFromNewSlab(
    SizeClass size_class) {
  using AllocRes = std::pair<PageId, SmallSlab*>;
  DEFINE_OR_RETURN_OPT(AllocRes, result,
                       slab_manager_->template Alloc<SmallSlab>(1, size_class));
  auto [page_id, slab] = result;

  CK_ASSERT_EQ(FreelistHead(size_class), PageId::Nil());
  AddToFreelist(slab);
  return TakeSlice(slab);
}

void SmallFreelist::ReturnSlice(SmallSlab* slab, AllocatedSlice* slice) {
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

PageId& SmallFreelist::FreelistHead(SizeClass size_class) {
  return freelists_[size_class.Ordinal()];
}

void SmallFreelist::AddToFreelist(SmallSlab* slab) {
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

void SmallFreelist::RemoveFromFreelist(SmallSlab* slab) {
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

}  // namespace ckmalloc

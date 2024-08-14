#include "src/ckmalloc/small_freelist.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slice.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

AllocatedSlice* SmallFreelist::TakeSlice(SmallSlab* slab) {
  CK_ASSERT_FALSE(slab->Metadata().Full());
  return slab->Metadata().PopSlice(
      [this, slab](SliceId slice_id) -> FreeSlice* {
        return static_cast<FreeSlice*>(SliceAt(slab, slice_id));
      });
}

void SmallFreelist::ReturnSlice(SmallSlab* slab, AllocatedSlice* slice) {
  CK_ASSERT_EQ(slab->Pages(), 1);
  void* slab_start = slab_manager_->PageStartFromId(slab->StartId());
  CK_ASSERT_GE(slice, slab_start);
  CK_ASSERT_LE(slice, static_cast<void*>(
                          reinterpret_cast<uint8_t*>(slab_start) + kPageSize -
                          slab->Metadata().SizeClass().SliceSize()));

  slab->Metadata().PushSlice(
      slice->ToFree(), IdForSlice(slab, slice),
      [this, slab](SliceId slice_id) -> FreeSlice* {
        return static_cast<FreeSlice*>(SliceAt(slab, slice_id));
      });
}

Slice* SmallFreelist::SliceAt(SmallSlab* slab, SliceId slice_id) {
  CK_ASSERT_NE(slice_id, SliceId::Nil());

  return reinterpret_cast<FreeSlice*>(
      reinterpret_cast<uint8_t*>(
          slab_manager_->PageStartFromId(slab->StartId())) +
      slice_id.SliceOffsetBytes(slab->Metadata().SizeClass()));
}

SliceId SmallFreelist::IdForSlice(SmallSlab* slab, Slice* slice) {
  return SliceId(
      PtrDistance(slice, slab_manager_->PageStartFromId(slab->StartId())) /
      kDefaultAlignment);
}

}  // namespace ckmalloc

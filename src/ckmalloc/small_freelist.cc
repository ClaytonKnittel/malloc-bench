#include "src/ckmalloc/small_freelist.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slice.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

AllocatedSlice* SmallFreelist::TakeSlice(SmallSlab* slab) {
  CK_ASSERT_FALSE(slab->Full());
  return slab->PopSlice(slab_manager_->PageStartFromId(slab->StartId()));
}

void SmallFreelist::ReturnSlice(SmallSlab* slab, AllocatedSlice* slice) {
  CK_ASSERT_EQ(slab->Pages(), 1);
  void* slab_start = slab_manager_->PageStartFromId(slab->StartId());
  CK_ASSERT_GE(slice, slab_start);
  CK_ASSERT_LE(
      slice, PtrAdd<AllocatedSlice>(slab_start,
                                    kPageSize - slab->SizeClass().SliceSize()));

  slab->PushSlice(slab_manager_->PageStartFromId(slab->StartId()), slice);
}

}  // namespace ckmalloc

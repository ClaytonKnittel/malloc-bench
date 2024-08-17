#pragma once

#include <optional>

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/size_class.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_manager.h"
#include "src/ckmalloc/slab_map.h"
#include "src/ckmalloc/slice.h"

namespace ckmalloc {

class SmallFreelist {
 public:
  explicit SmallFreelist(SlabManager* slab_manager, SlabMap* slab_map)
      : slab_manager_(slab_manager), slab_map_(slab_map) {}

  AllocatedSlice* AllocSlice(size_t user_size);

  void FreeSlice(AllocatedSlice* slice);

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

  SlabManager* const slab_manager_;
  SlabMap* const slab_map_;

  static_assert(SizeClass::kNumSizeClasses == 9);
  PageId freelists_[SizeClass::kNumSizeClasses] = {
    PageId::Nil(), PageId::Nil(), PageId::Nil(), PageId::Nil(), PageId::Nil(),
    PageId::Nil(), PageId::Nil(), PageId::Nil(), PageId::Nil(),
  };
};

}  // namespace ckmalloc

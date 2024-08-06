#pragma once

#include <cstdint>

#include "src/ckmalloc/common.h"

namespace ckmalloc {

class SlabId {
  friend class SlabManager;

  template <AllocFn Alloc>
  friend class SlabMapImpl;

 public:
  SlabId(const SlabId& slab_id) = default;
  SlabId& operator=(const SlabId& slab_id) = default;

  SlabId operator+(uint32_t offset) const {
    return SlabId(slab_idx_ + offset);
  }

  // The id of the first slab in the heap. This is reserved for the first
  // metadata slab.
  static constexpr SlabId Zero() {
    return SlabId(0);
  }

 private:
  constexpr explicit SlabId(uint32_t slab_idx) : slab_idx_(slab_idx) {}

  uint32_t Idx() const {
    return slab_idx_;
  }

  // The index into the heap of this slab, where idx 0 is the first pagesize
  // slab, idx 1 is the next pagesize slab, and so on.
  uint32_t slab_idx_;
};

}  // namespace ckmalloc

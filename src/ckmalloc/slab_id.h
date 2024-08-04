#pragma once

#include <cstdint>

namespace ckmalloc {

class SlabId {
  friend class SlabManger;

 public:
  SlabId(const SlabId& slab_id) = default;
  SlabId& operator=(const SlabId& slab_id) = default;

 private:
  explicit SlabId(uint32_t slab_idx) : slab_idx_(slab_idx) {}

  uint32_t Idx() const {
    return slab_idx_;
  }

  // The index into the heap of this slab, where idx 0 is the first pagesize
  // slab, idx 1 is the next pagesize slab, and so on.
  uint32_t slab_idx_;
};

}  // namespace ckmalloc

#pragma once

#include <cstdint>
#include <limits>

#include "src/ckmalloc/common.h"

namespace ckmalloc {

// Slice id's are offsets from the beginning of the slab of the slice in a
// small slab, in multiples of `kMinAlignment`.
class SliceId {
  friend class SmallSlab;

 public:
  explicit SliceId(uint16_t id) : id_(id) {}

  // Allow copy construction/assignment.
  SliceId(const SliceId&) = default;
  SliceId& operator=(const SliceId&) = default;

  static SliceId Nil() {
    return SliceId(kNilId);
  }

  uint16_t Id() const {
    return id_;
  }

 private:
  static constexpr uint16_t kNilId = std::numeric_limits<uint16_t>::max();

  uint16_t id_;
};

// Free slices are unallocated slices in small slabs which hold some metadata.
// Together they form the freelist within that slab.
class FreeSlice {
 private:
  SliceId slices_[4];
};

// Free slices must be able to fit in any small slab.
static_assert(sizeof(FreeSlice) <= kMinAlignment);

}  // namespace ckmalloc

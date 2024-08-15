#pragma once

#include <cstdint>
#include <limits>
#include <ostream>

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/size_class.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

// Slice id's are offsets from the beginning of the slab of the slice in a
// small slab, in multiples of `kMinAlignment`.
class SliceId {
  friend class SmallSlab;

  friend inline std::ostream& operator<<(std::ostream&, const SliceId&);

 public:
  explicit SliceId(uint16_t idx) : id_(idx) {}

  // Allow copy construction/assignment.
  SliceId(const SliceId&) = default;
  SliceId& operator=(const SliceId&) = default;

  static SliceId Nil() {
    return SliceId(kNilId);
  }

  bool operator==(const SliceId& other) const {
    return id_ == other.id_;
  }
  bool operator!=(const SliceId& other) const {
    return !(*this == other);
  }

  uint32_t SliceOffsetBytes(SizeClass size_class) const {
    return id_ * size_class.SliceSize();
  }

 private:
  static constexpr uint16_t kNilId = std::numeric_limits<uint16_t>::max();

  // The index of the slice in the slab.
  uint16_t id_;
};

inline std::ostream& operator<<(std::ostream& ostr, const SliceId& slice_id) {
  return ostr << slice_id.id_;
}

class Slice {};

// Free slices are unallocated slices in small slabs which hold some metadata.
// Together they form the freelist within that slab.
class FreeSlice : public Slice {
 public:
  SliceId& IdAt(uint8_t offset) {
    return slices_[offset];
  }

  void SetId(uint8_t offset, SliceId slice_id) {
    slices_[offset] = slice_id;
  }

  class AllocatedSlice* ToAllocated() {
    return reinterpret_cast<class AllocatedSlice*>(this);
  }

 private:
  SliceId slices_[];
};

// Allocated slices have no metadata.
class AllocatedSlice : public Slice {
 public:
  // Returns a pointer to the beginning of the user-allocatable region of memory
  // in this slice, which is the whole slice.
  void* UserDataPtr() {
    return this;
  }

  // Given a user data pointer, returns the allocated slice containing this
  // pointer.
  static AllocatedSlice* FromUserDataPtr(void* ptr) {
    return reinterpret_cast<AllocatedSlice*>(ptr);
  }

  FreeSlice* ToFree() {
    return reinterpret_cast<FreeSlice*>(this);
  }
};

}  // namespace ckmalloc

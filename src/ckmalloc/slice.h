#pragma once

#include <cstdint>
#include <type_traits>

#include "src/ckmalloc/slice_id.h"

namespace ckmalloc {

class Slice {};

// Free slices are unallocated slices in small slabs which hold some metadata.
// Together they form the freelist within that slab.
template <typename T>
requires std::is_integral_v<T>
class FreeSlice : public Slice {
 public:
  SliceId<T>& IdAt(uint8_t offset) {
    return slices_[offset];
  }

  void SetId(uint8_t offset, SliceId<T> slice_id) {
    slices_[offset] = slice_id;
  }

  class AllocatedSlice* ToAllocated() {
    return reinterpret_cast<class AllocatedSlice*>(this);
  }

 private:
  SliceId<T> slices_[];
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

  template <typename T>
  requires std::is_integral_v<T>
  FreeSlice<T>* ToFree() {
    return reinterpret_cast<FreeSlice<T>*>(this);
  }
};

}  // namespace ckmalloc

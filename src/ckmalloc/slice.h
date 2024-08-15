#pragma once

#include <cstdint>

#include "src/ckmalloc/slice_id.h"

namespace ckmalloc {

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

#pragma once

#include <cstddef>
#include <new>

#include "src/fake_heap.h"

namespace ckmalloc {

// Alloc-only allocator that allocates memory directly from the kernel.
class FakeAllocator {
 public:
  FakeAllocator();

  // Allocates `size` bytes aligned to `alignment` and returns a pointer to the
  // beginning of that region.
  //
  // If out of memory, `nullptr` is returned.
  void* Alloc(size_t size, std::align_val_t alignment);

 private:
  void* region_start_;
  void* region_end_;

  static constexpr size_t kRegionSize = bench::FakeHeap::kHeapSize;
};

}  // namespace ckmalloc

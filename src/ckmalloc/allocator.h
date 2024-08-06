#pragma once

#include <cstddef>
#include <new>

#include "src/heap_interface.h"

namespace ckmalloc {

// TODO delete if not used
// Alloc-only allocator that allocates memory directly from the kernel.
class Allocator {
 public:
  // Must be constructed when the heap is empty.
  explicit Allocator(bench::Heap* heap);

  // Allocates `size` bytes aligned to `alignment` and returns a pointer to the
  // beginning of that region.
  //
  // If out of memory, `nullptr` is returned.
  void* Alloc(size_t size, size_t alignment = 1);

  size_t AllocatedBytes() const;

 private:
  bench::Heap* heap_;
};

}  // namespace ckmalloc

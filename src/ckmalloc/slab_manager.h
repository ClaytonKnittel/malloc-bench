#pragma once

#include <cstddef>

#include "src/heap_interface.h"

namespace ckmalloc {

class SlabManager {
 public:
  explicit SlabManager(bench::Heap* heap);

  // Allocates `n_pages` contiguous pages, if there is availability, otherwise
  // returning `nullptr`.
  void* Alloc(size_t n_pages);

  // The size of slabs in bytes. Typically page size.
  static const size_t kSlabSize;

 private:
  // The heap that this SlabManager allocates slabs from.
  bench::Heap* heap_;
};

}  // namespace ckmalloc

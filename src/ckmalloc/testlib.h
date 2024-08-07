#pragma once

#include <cstddef>
#include <new>

#include "src/ckmalloc/slab_map.h"

namespace ckmalloc {

void* Allocate(size_t size, size_t alignment) {
  return ::operator new(size, static_cast<std::align_val_t>(alignment));
}

using TestSlabMap = SlabMapImpl<Allocate>;

}  // namespace ckmalloc

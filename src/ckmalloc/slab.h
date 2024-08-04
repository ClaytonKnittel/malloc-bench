#pragma once

#include <cstdint>

#include "src/ckmalloc/slab_id.h"

namespace ckmalloc {

class Slab {
 public:
  static Slab* MakeSmallBlocksSlab();

 private:
  SlabId id_;
  uint32_t n_pages_;
};

}  // namespace ckmalloc

#pragma once

#include "src/ckmalloc/slab_manager.h"

namespace ckmalloc {

class HeapManager {
 public:
  HeapManager();

 private:
  SlabManager* slab_manager_;
};

}  // namespace ckmalloc

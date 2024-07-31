#pragma once

#include "src/heap_interface.h"

namespace ckmalloc {

class HeapManager {
 public:
  HeapManager();

 private:
  class SlabManager* slab_manager_;
};

}  // namespace ckmalloc

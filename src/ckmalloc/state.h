#pragma once

#include "src/ckmalloc/metadata_manager.h"
#include "src/ckmalloc/slab_manager.h"
#include "src/heap_interface.h"

namespace ckmalloc {

class State {
 public:
  // Initializes a new `State` with a heap has not been allocated from yet. The
  // `State` takes ownership of the heap.
  static State* InitializeWithEmptyAlloc(bench::Heap* heap);

 private:
  explicit State(bench::Heap* heap);

  SlabManager slab_manager_;
  MetadataManager metadata_manager_;
};

}  // namespace ckmalloc

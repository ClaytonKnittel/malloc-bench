#pragma once

#include "src/ckmalloc/main_allocator.h"
#include "src/ckmalloc/metadata_manager.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab_manager.h"
#include "src/ckmalloc/slab_map.h"
#include "src/ckmalloc/small_allocator.h"
#include "src/heap_factory.h"

namespace ckmalloc {

class State {
 public:
  // Initializes a new `State` with a heap has not been allocated from yet. The
  // `State` takes ownership of the heap.
  static State* InitializeWithEmptyHeap(bench::HeapFactory* heap_factory);

  // Returns the singleton `State` instance.
  static State* Instance();

  SlabMap* SlabMap() {
    return &slab_map_;
  }

  SlabManager* SlabManager() {
    return &slab_manager_;
  }

  MetadataManager* MetadataManager() {
    return &metadata_manager_;
  }

  MainAllocator* MainAllocator() {
    return &main_allocator_;
  }

 private:
  explicit State(bench::HeapFactory* heap_factory, PageId last);

  // This is the global state instance that is initialized with
  // `InitializeWithEmptyHeap`.
  static State* state_;

  ckmalloc::SlabMap slab_map_;
  ckmalloc::SlabManager slab_manager_;
  ckmalloc::MetadataManager metadata_manager_;
  ckmalloc::SmallAllocator small_alloc_;
  ckmalloc::MainAllocator main_allocator_;
};

}  // namespace ckmalloc

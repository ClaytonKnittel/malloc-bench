#include "src/ckmalloc/global_state.h"

#include "src/ckmalloc/main_allocator.h"
#include "src/ckmalloc/metadata_manager.h"

namespace ckmalloc {

GlobalState::GlobalState(bench::Heap* meta_heap, bench::Heap* alloc_heap)
    : slab_manager_(alloc_heap, &slab_map_),
      metadata_manager_(meta_heap, &slab_map_),
      small_alloc_(&slab_map_, &slab_manager_),
      large_alloc_(&slab_map_, &slab_manager_),
      main_allocator_(&slab_map_, &slab_manager_, &small_alloc_,
                      &large_alloc_) {}

}  // namespace ckmalloc

#include "src/ckmalloc/global_state.h"

#include "src/ckmalloc/main_allocator.h"
#include "src/ckmalloc/metadata_manager.h"
#include "src/heap_interface.h"

namespace ckmalloc {

GlobalState::GlobalState(bench::Heap* metadata_heap, bench::Heap* user_heap)
    : slab_manager_(user_heap, &slab_map_),
      metadata_manager_(metadata_heap, &slab_map_),
      small_alloc_(&slab_map_, &slab_manager_, &freelist_),
      large_alloc_(&slab_map_, &slab_manager_, &freelist_),
      main_allocator_(&slab_map_, &slab_manager_, &small_alloc_,
                      &large_alloc_) {}

}  // namespace ckmalloc

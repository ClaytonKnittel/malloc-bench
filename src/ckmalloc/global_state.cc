#include "src/ckmalloc/global_state.h"

#include "src/ckmalloc/main_allocator.h"
#include "src/ckmalloc/metadata_manager.h"

namespace ckmalloc {

GlobalState::GlobalState(void* metadata_heap, void* metadata_heap_end,
                         void* user_heap)
    : slab_manager_(user_heap, &slab_map_),
      metadata_manager_(metadata_heap, metadata_heap_end, &slab_map_),
      small_alloc_(&slab_map_, &slab_manager_, &freelist_),
      large_alloc_(&slab_map_, &slab_manager_, &freelist_),
      main_allocator_(&slab_map_, &slab_manager_, &small_alloc_,
                      &large_alloc_) {}

}  // namespace ckmalloc

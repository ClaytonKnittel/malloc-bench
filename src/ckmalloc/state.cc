#include "src/ckmalloc/state.h"

#include "src/ckmalloc/main_allocator.h"
#include "src/ckmalloc/metadata_manager.h"
#include "src/heap_factory.h"

namespace ckmalloc {

State::State(bench::HeapFactory* heap_factory, size_t metadata_heap_idx,
             size_t user_heap_idx)
    : slab_manager_(heap_factory, &slab_map_, user_heap_idx),
      metadata_manager_(heap_factory, &slab_map_, metadata_heap_idx),
      small_alloc_(&slab_map_, &slab_manager_),
      large_alloc_(&slab_map_, &slab_manager_),
      main_allocator_(&slab_map_, &slab_manager_, &small_alloc_,
                      &large_alloc_) {}

}  // namespace ckmalloc

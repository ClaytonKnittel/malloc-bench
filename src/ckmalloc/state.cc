#include "src/ckmalloc/state.h"

#include "src/ckmalloc/metadata_manager.h"
#include "src/ckmalloc/slab_id.h"
#include "src/ckmalloc/slab_manager.h"
#include "src/heap_interface.h"

namespace ckmalloc {

/* static */
State* State::InitializeWithEmptyAlloc(bench::Heap* heap) {
  CK_ASSERT(heap->Size() == 0);
  // Allocate a metadata slab and place ourselves at the beginning of it.
  void* heap_start = heap->sbrk(SlabManager::kSlabSize);
  auto* slab_manager = new (heap_start) State(heap);
  return slab_manager;
}

State::State(bench::Heap* heap)
    : slab_manager_(heap), metadata_manager_(SlabId::Zero()) {
  // Allocate a single metadata slab.
  slab_manager_.Alloc(1);
}

}  // namespace ckmalloc

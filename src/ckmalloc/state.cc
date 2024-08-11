#include "src/ckmalloc/state.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/metadata_manager.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_manager.h"
#include "src/ckmalloc/util.h"
#include "src/heap_interface.h"

namespace ckmalloc {

State* State::state_ = nullptr;

Slab* GlobalMetadataAlloc::SlabAlloc() {
  return State::Instance()->MetadataManager()->NewSlabMeta();
}

void GlobalMetadataAlloc::SlabFree(MappedSlab* slab) {
  State::Instance()->MetadataManager()->FreeSlabMeta(slab);
}

void* GlobalMetadataAlloc::Alloc(size_t size, size_t alignment) {
  return State::Instance()->MetadataManager()->Alloc(size, alignment);
}

/* static */
State* State::InitializeWithEmptyHeap(bench::Heap* heap) {
  CK_ASSERT(heap->Size() == 0);
  static_assert(sizeof(State) <= kPageSize,
                "sizeof(State) is larger than page size");
  // Allocate a metadata slab and place ourselves at the beginning of it.
  void* heap_start = heap->sbrk(kPageSize);
  auto* state = new (heap_start) State(heap);
  state_ = state;
  return state;
}

State* State::Instance() {
  CK_ASSERT(state_ != nullptr);
  return state_;
}

State::State(bench::Heap* heap)
    : slab_manager_(heap, &slab_map_),
      metadata_manager_(&slab_map_, &slab_manager_),
      freelist_(&slab_map_, &slab_manager_) {}

}  // namespace ckmalloc

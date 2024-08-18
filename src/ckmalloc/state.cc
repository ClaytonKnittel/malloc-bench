#include "src/ckmalloc/state.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/main_allocator.h"
#include "src/ckmalloc/metadata_manager.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_manager.h"
#include "src/ckmalloc/slab_map.h"
#include "src/heap_interface.h"

namespace ckmalloc {

State* State::state_ = nullptr;

/* static */
State* State::InitializeWithEmptyHeap(bench::Heap* heap) {
  CK_ASSERT_EQ(heap->Size(), 0);
  static_assert(sizeof(State) <= kPageSize,
                "sizeof(State) is larger than page size");
  // Allocate a metadata slab and place ourselves at the beginning of it.
  void* heap_start = heap->sbrk(kPageSize);
  auto* state = new (heap_start) State(heap);
  state_ = state;
  return state;
}

/* static */
State* State::Instance() {
  CK_ASSERT_NE(state_, nullptr);
  return state_;
}

State::State(bench::Heap* heap)
    : slab_manager_(heap, &slab_map_),
      metadata_manager_(&slab_map_, &slab_manager_ /*, sizeof(State)*/),
      small_alloc_(&slab_map_, &slab_manager_),
      main_allocator_(&slab_map_, &slab_manager_, &small_alloc_) {}

// template <>
// TestState* TestState::state_ = nullptr;

Slab* GlobalMetadataAlloc::SlabAlloc() {
  return State::Instance()->MetadataManager()->NewSlabMeta();
}

void GlobalMetadataAlloc::SlabFree(MappedSlab* slab) {
  State::Instance()->MetadataManager()->FreeSlabMeta(slab);
}

void* GlobalMetadataAlloc::Alloc(size_t size, size_t alignment) {
  return State::Instance()->MetadataManager()->Alloc(size, alignment);
}

}  // namespace ckmalloc

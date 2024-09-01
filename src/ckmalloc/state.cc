#include "src/ckmalloc/state.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/main_allocator.h"
#include "src/ckmalloc/metadata_manager.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_map.h"
#include "src/ckmalloc/util.h"
#include "src/heap_factory.h"
#include "src/heap_interface.h"

namespace ckmalloc {

State* State::state_ = nullptr;

/* static */
State* State::InitializeWithEmptyHeap(bench::HeapFactory* heap_factory) {
  CK_ASSERT_EQ(heap_factory->Instance(0), nullptr);
  auto result = heap_factory->NewInstance(kHeapSize);
  CK_ASSERT_TRUE(result.ok());
  // Allocate a metadata slab and place ourselves at the beginning of it.
  size_t metadata_size = AlignUp(sizeof(State), kPageSize);
  void* heap_start = heap_factory->Instance(0)->sbrk(metadata_size);
  CK_ASSERT_NE(heap_start, nullptr);

  auto* state = new (heap_start) State(heap_factory);
  state_ = state;
  return state;
}

/* static */
State* State::Instance() {
  CK_ASSERT_NE(state_, nullptr);
  return state_;
}

State::State(bench::HeapFactory* heap_factory)
    : slab_manager_(heap_factory, &slab_map_),
      metadata_manager_(heap_factory, &slab_map_),
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

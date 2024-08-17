#pragma once

#include "src/ckmalloc/main_allocator.h"
#include "src/ckmalloc/metadata_manager.h"
#include "src/ckmalloc/slab_manager.h"
#include "src/ckmalloc/slab_map.h"
#include "src/heap_interface.h"

namespace ckmalloc {

template <SlabMapInterface SlabMapT, SlabManagerInterface SlabManagerT>
class StateImpl {
 public:
  // Initializes a new `State` with a heap has not been allocated from yet. The
  // `State` takes ownership of the heap.
  static StateImpl* InitializeWithEmptyHeap(bench::Heap* heap);

  // Returns the singleton `State` instance.
  static StateImpl* Instance();

  SlabMapT* SlabMap() {
    return &slab_map_;
  }

  SlabManagerT* SlabManager() {
    return &slab_manager_;
  }

  MetadataManagerImpl<SlabMapT, SlabManagerT>* MetadataManager() {
    return &metadata_manager_;
  }

  MainAllocatorImpl<SlabMapT, SlabManagerT>* MainAllocator() {
    return &main_allocator_;
  }

 private:
  explicit StateImpl(bench::Heap* heap);

  // This is the global state instance that is initialized with
  // `InitializeWithEmptyHeap`.
  static StateImpl* state_;

  SlabMapT slab_map_;
  SlabManagerT slab_manager_;
  MetadataManagerImpl<SlabMapT, SlabManagerT> metadata_manager_;
  MainAllocatorImpl<SlabMapT, SlabManagerT> main_allocator_;
};

/* static */
template <SlabMapInterface SlabMapT, SlabManagerInterface SlabManagerT>
StateImpl<SlabMapT, SlabManagerT>*
StateImpl<SlabMapT, SlabManagerT>::InitializeWithEmptyHeap(bench::Heap* heap) {
  CK_ASSERT_EQ(heap->Size(), 0);
  static_assert(sizeof(StateImpl<SlabMapT, SlabManagerT>) <= kPageSize,
                "sizeof(State) is larger than page size");
  // Allocate a metadata slab and place ourselves at the beginning of it.
  void* heap_start = heap->sbrk(kPageSize);
  auto* state = new (heap_start) StateImpl<SlabMapT, SlabManagerT>(heap);
  state_ = state;
  return state;
}

/* static */
template <SlabMapInterface SlabMapT, SlabManagerInterface SlabManagerT>
StateImpl<SlabMapT, SlabManagerT>*
StateImpl<SlabMapT, SlabManagerT>::Instance() {
  CK_ASSERT_NE(state_, nullptr);
  return state_;
}

template <SlabMapInterface SlabMapT, SlabManagerInterface SlabManagerT>
StateImpl<SlabMapT, SlabManagerT>::StateImpl(bench::Heap* heap)
    : slab_manager_(heap, &slab_map_),
      metadata_manager_(&slab_map_, &slab_manager_ /*, sizeof(State)*/),
      main_allocator_(&slab_map_, &slab_manager_) {}

using State = StateImpl<SlabMap, SlabManager>;

template <>
State* State::state_;

}  // namespace ckmalloc

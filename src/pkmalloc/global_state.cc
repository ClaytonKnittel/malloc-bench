#include "src/pkmalloc/global_state.h"

#include "src/heap_factory.h"
#include "src/heap_interface.h"

namespace pkmalloc {

class FreeBlock;

void* GlobalState::GetHeapStart(bench::Heap* heap) {
  GlobalState* global_heap_start = static_cast<GlobalState*>(heap->Start());
  return global_heap_start->heap_start_ptr_;
}

bench::Heap* GlobalState::GetGlobalHeapStart(GlobalState* global_state) {
  auto* global_heap_start =
      static_cast<bench::Heap*>(global_state->heap_start_ptr_);
  // THIS MATH MIGHT BE WRONG
  return global_heap_start - 1;
}

void* GlobalState::GetHeapEnd(bench::Heap* heap) {
  return heap->End();
}

void GlobalState::SetHeapStart(bench::Heap* heap) {
  // allocate space for global state ptrs at beginning of heap
  GlobalState* global_heap_start = static_cast<GlobalState*>(
      heap->sbrk(static_cast<intptr_t>(sizeof(GlobalState))));
  // set the global ptrs to point to these locations
  global_heap_start->heap_start_ptr_ = global_heap_start + 1;
  global_heap_start->free_list_start_ptr_ = nullptr;
}

FreeBlock* GlobalState::GetFreeListStart(GlobalState* global_state) {
  if (global_state == nullptr) {
    return nullptr;
  }
  auto* free_list_start_ptr = global_state->free_list_start_ptr_;
  if (free_list_start_ptr != nullptr) {
    return reinterpret_cast<FreeBlock*>(free_list_start_ptr);
  }
  return nullptr;
}

void GlobalState::SetFreeListStart(FreeList* free_list_start,
                                   GlobalState* global_state) {
  global_state->free_list_start_ptr_ = free_list_start;
}

}  // namespace pkmalloc
#include "src/pkmalloc/global_state.h"

#include "src/heap_factory.h"
#include "src/heap_interface.h"
#include "src/pkmalloc/free_block.h"

namespace pkmalloc {
Block* get_heap_start(bench::Heap* heap) {
  GlobalState* global_heap_start = static_cast<GlobalState*>(heap->Start());
  return reinterpret_cast<Block*>(global_heap_start);
}

void* get_heap_end(bench::Heap* global_heap_start_ptr, int heap_size) {
  // heap size should be 524288000
  return global_heap_start_ptr + heap_size;
}

void set_heap_start(bench::Heap* heap) {
  // allocate space for global state ptrs at beginning of heap
  GlobalState* global_heap_start = static_cast<GlobalState*>(
      heap->sbrk(static_cast<intptr_t>(sizeof(GlobalState))));
  // set the global ptrs to point to these locations
  global_heap_start->heap_start_ptr =
      reinterpret_cast<Block*>(global_heap_start + 1);
  global_heap_start->free_list_start_ptr = nullptr;
}

FreeBlock* get_free_list_start(bench::Heap* heap) {
  GlobalState* global_heap_start = static_cast<GlobalState*>(heap->Start());
  return reinterpret_cast<FreeBlock*>(
      reinterpret_cast<uint8_t*>(global_heap_start) + 1);
}

void set_free_list_start(FreeBlock* free_list_start, bench::Heap* heap) {
  // free_list_start_ptr = free_list_start;
  GlobalState* global_heap_start = static_cast<GlobalState*>(heap->Start());
  FreeBlock* free_list_start_ptr = reinterpret_cast<FreeBlock*>(
      reinterpret_cast<uint8_t*>(global_heap_start) + 1);
  free_list_start_ptr = free_list_start;
}

}  // namespace pkmalloc
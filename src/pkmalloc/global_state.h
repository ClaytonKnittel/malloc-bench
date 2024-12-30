#pragma once

#include "src/heap_factory.h"
#include "src/heap_interface.h"

namespace pkmalloc {

class FreeBlock;
class FreeList;

class GlobalState {
 public:
  // global information stored at the beginning of the heap

  // returns block pointer to start of heap, beyond global stored info, where
  // alloc and free spaces will exist
  void* GetHeapStart(bench::Heap* heap);

  bench::Heap* GetGlobalHeapStart(GlobalState* global_state);

  // returns the end of the heap to user
  static void* GetHeapEnd(bench::Heap* heap);

  // initialize global state object located at heap start
  void SetHeapStart(bench::Heap* heap);

  // struct at beginning of the heap stores this pointer, here, it is updated
  // returns ptr to beginning of free list
  FreeBlock* GetFreeListStart(GlobalState* global_state);

  // returns a free block pointer to the first free block in the free list, null
  // if free list is empty
  void SetFreeListStart(FreeList* free_list_start, GlobalState* global_state);

 private:
  // global ptr to start of heap
  void* heap_start_ptr_ = nullptr;
  // global ptr to start of free list
  FreeList* free_list_start_ptr_ = nullptr;
};

}  // namespace pkmalloc
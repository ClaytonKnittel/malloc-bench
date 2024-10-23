#include "src/heap_factory.h"
#include "src/heap_interface.h"
#include "src/pkmalloc/free_block.h"

namespace pkmalloc {

struct GlobalState {
  // global ptr to start of heap
  Block* heap_start_ptr = nullptr;
  // global ptr to start of free list
  FreeBlock* free_list_start_ptr = nullptr;
};

// global information stored at the beginning of the heap

// returns block pointer to start of heap, beyond global stored info, where
// alloc and free spaces will exist
Block* get_heap_start(bench::Heap* heap);

// returns the end of the heap to user
void* get_heap_end(bench::Heap* heap);

// initialize global state object located at heap start
void set_heap_start(bench::Heap* heap);

// returns a free block pointer to the first free block in the free list, null
// if free list is empty
FreeBlock* get_free_list_start(bench::Heap* heap);

// struct at beginning of the heap stores this pointer, here, it is updated
// returns ptr to beginning of free list
void set_free_list_start(FreeBlock* free_list_start, bench::Heap* heap);

}  // namespace pkmalloc
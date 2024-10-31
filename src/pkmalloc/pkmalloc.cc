#include "src/pkmalloc/pkmalloc.h"

#include <cassert>
#include <cstddef>
#include <cstring>
#include <iostream>

#include "src/heap_factory.h"
#include "src/heap_interface.h"
#include "src/pkmalloc/allocated_block.h"
#include "src/pkmalloc/free_block.h"
#include "src/pkmalloc/free_list.h"
#include "src/pkmalloc/global_state.h"

namespace pkmalloc {

// global ptr to start of heap
// bench::Heap* heap = nullptr;

// maybe instead have global GlobalState ptr instead of heap

GlobalState* global_state = nullptr;

void PkMalloc::initialize_heap(bench::HeapFactory& heap_factory) {
  // heap size: 500MB
  auto result = heap_factory.NewInstance(524288000);
  if (!result.ok()) {
    std::cerr << "Failed to make new heap: " << result.status() << std::endl;
    std::abort();
  }
  bench::Heap* heap = result.value();
  global_state->GlobalState::SetHeapStart(heap);
}

// more asserts ??
// check for consistency on size w and wo header
// make sure int types are good

void* PkMalloc::malloc(size_t size, size_t alignment) {
  (void) alignment;
  if (size == 0) {
    return nullptr;
  }
  AllocatedBlock* result_block =
      pkmalloc::FreeList::mallocate(size, global_state);
  return result_block->GetBody();
}

void* PkMalloc::calloc(size_t nmemb, size_t size) {
  // TODO: implement
  void* ptr = malloc(nmemb * size);
  memset(ptr, 0, nmemb * size);
  return ptr;
}

void* PkMalloc::realloc(void* ptr, size_t size) {
  // TODO: implement
  void* new_ptr = malloc(size);
  if (ptr != nullptr) {
    memcpy(new_ptr, ptr, size);
  }
  return new_ptr;
}

// when calling coalesce, make sure to check direct address neighbors, not free
// block list
void PkMalloc::free(void* ptr, size_t size_hint, size_t alignment_hint) {
  (void) size_hint;
  (void) alignment_hint;
  if (ptr == nullptr) {
    return;
  }
  AllocatedBlock* block = AllocatedBlock::FromRawPtr(ptr);
  pkmalloc::FreeList::AddFreeBlockToList(block, global_state);
}

size_t PkMalloc::get_size(void* ptr) {
  // TODO: implement
  (void) ptr;
  return 0;
}

}  // namespace pkmalloc
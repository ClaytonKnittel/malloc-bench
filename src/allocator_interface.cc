<<<<<<< HEAD
#include "src/allocator_interface.h"

#include <mutex>
#include <optional>

#include "src/heap_interface.h"
#include "src/mmap_heap.h"

namespace bench {

namespace {

std::optional<MMapHeap> heap;

}

Heap* g_heap = nullptr;

std::mutex g_lock;

void initialize() {
  auto res = MMapHeap::New(kHeapSize);
  heap.emplace(std::move(res.value()));
  g_heap = &heap.value();
}

}  // namespace bench
=======

#include "src/allocator_interface.h"

#include <cassert>
#include <cstddef>
#include <cstring>

#include "src/pkmalloc/allocated_block.h"
#include "src/pkmalloc/free_block.h"
#include "src/pkmalloc/free_list.h"

FreeBlock* heap_metadata_start = nullptr;

void* initialize_heap() {
  // do stuff
}

// more asserts ??
// check for consistency on size w and wo header
// make sure int types are good

inline void* bench::malloc(size_t size) {
  if (size == 0) {
    return nullptr;
  }
  AllocatedBlock* result_block = FreeList::mallocate(size, heap_metadata_start);
  return result_block->GetBody();
}

inline void* bench::calloc(size_t nmemb, size_t size) {
  // TODO: implement
  void* ptr = malloc(nmemb * size);
  memset(ptr, 0, nmemb * size);
  return ptr;
}

inline void* bench::realloc(void* ptr, size_t size) {
  // TODO: implement
  void* new_ptr = malloc(size);
  if (ptr != nullptr) {
    memcpy(new_ptr, ptr, size);
  }
  return new_ptr;
}

// when calling coalesce, make sure to check direct address neighbors, not free
// block list
inline void bench::free(void* ptr) {
  if (ptr == nullptr) {
    return;
  }
  AllocatedBlock* block = AllocatedBlock::FromRawPtr(ptr);
  FreeList::add_free_block_to_list(block, heap_metadata_start);
}
>>>>>>> d3b973fd6e938786ae4ec0560b204de2d3ba8e58

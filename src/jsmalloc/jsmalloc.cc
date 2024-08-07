#include "src/jsmalloc/jsmalloc.h"

#include <cassert>
#include <cstddef>
#include <new>

#include "src/heap_interface.h"
#include "src/jsmalloc/chunky_block.h"
#include "src/jsmalloc/util/math.h"
#include "src/jsmalloc/util/assert.h"

namespace jsmalloc {

namespace {

class HeapMetadata {
 public:
  Block::FreeList free_list;
};

HeapMetadata* heap_metadata = nullptr;

}  // namespace

void* sbrk_16b(bench::Heap& heap, size_t size) {
  DCHECK_EQ(size % 16, 0);
  return heap.sbrk(size);
}

// Called before any allocations are made.
void initialize_heap(bench::Heap& heap) {
  heap_metadata = new (sbrk_16b(heap, math::round_16b(sizeof(HeapMetadata))))
      HeapMetadata();

  size_t initial_block_padding =
      math::round_16b(Block::DataOffset()) - Block::DataOffset();
  heap.sbrk(initial_block_padding);
}

void* malloc(bench::Heap& heap, size_t size) {
  if (size == 0) {
    return nullptr;
  }

  for (Block& free_block : heap_metadata->free_list) {
    if (free_block.DataSize() >= size) {
      heap_metadata->free_list.remove(free_block);
      return free_block.Data();
    }
  }

  size_t block_size = Block::SizeForUserData(size);
  auto* block = new (sbrk_16b(heap, block_size)) Block(block_size);
  return block->Data();
}

void* calloc(bench::Heap& heap, size_t nmemb, size_t size) {
  void* ptr = malloc(heap, nmemb * size);
  memset(ptr, 0, nmemb * size);
  return ptr;
}

void* realloc(bench::Heap& heap, void* ptr, size_t size) {
  void* new_ptr = malloc(heap, size);
  if (size > 0) {
    memcpy(new_ptr, ptr, size);
  }
  return new_ptr;
}

void free(void* ptr) {
  if (ptr == nullptr) {
    return;
  }
  auto* block = Block::FromDataPtr(ptr);
  heap_metadata->free_list.insert_back(*block);
}

}  // namespace jsmalloc

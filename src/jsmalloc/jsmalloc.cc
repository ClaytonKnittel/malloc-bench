#include "src/jsmalloc/jsmalloc.h"

#include <cassert>
#include <cstddef>

#include "src/heap_interface.h"
#include "src/jsmalloc/block.h"
#include "src/jsmalloc/mallocator.h"
#include "src/jsmalloc/util/assert.h"
#include "src/jsmalloc/util/math.h"

namespace jsmalloc {

namespace {

class HeapMetadata {
 public:
  LargeBlock::FreeList large_block_free_list;
  MultiSmallBlockFreeList small_block_free_list;
};

HeapMetadata* heap_metadata = nullptr;

void* malloc_large_block(bench::Heap& heap, size_t size) {
  for (LargeBlock& free_block : heap_metadata->large_block_free_list) {
    if (free_block.DataSize() >= size) {
      heap_metadata->large_block_free_list.remove(free_block);
      return free_block.Alloc();
    }
  }

  HeapMallocator mallocator(&heap);
  LargeBlock* block = LargeBlock::New(&mallocator, size);
  if (block == nullptr) {
    return nullptr;
  }
  return block->Alloc();
}

void* malloc_small_block(bench::Heap& heap, size_t size) {
  SmallBlock::FreeList& free_list =
      heap_metadata->small_block_free_list.Find(size);
  if (free_list.size() > 0) {
    SmallBlock* block = free_list.front();
    void* ptr = block->Alloc();
    if (!block->CanAlloc()) {
      free_list.remove(*block);
    }
    return ptr;
  }

  HeapMallocator mallocator(&heap);
  SmallBlock* block = MultiSmallBlockFreeList::Create(&mallocator, size);
  if (block == nullptr) {
    return nullptr;
  }
  void* ptr = block->Alloc();
  heap_metadata->small_block_free_list.EnsureContains(*block);
  return ptr;
}

}  // namespace

void* sbrk_16b(bench::Heap& heap, size_t size) {
  DCHECK_EQ(size % 16, 0);
  return heap.sbrk(size);
}

// Called before any allocations are made.
void initialize_heap(bench::Heap& heap) {
  heap_metadata = new (sbrk_16b(heap, math::round_16b(sizeof(HeapMetadata))))
      HeapMetadata();
}

void* malloc(bench::Heap& heap, size_t size) {
  if (size == 0) {
    return nullptr;
  }

  if (size > kMaxSmallBlockDataSize) {
    return malloc_large_block(heap, size);
  }
  return malloc_small_block(heap, size);
}

void* calloc(bench::Heap& heap, size_t nmemb, size_t size) {
  void* ptr = malloc(heap, nmemb * size);
  if (ptr == nullptr) {
    return nullptr;
  }
  memset(ptr, 0, nmemb * size);
  return ptr;
}

void* realloc(bench::Heap& heap, void* ptr, size_t size) {
  void* new_ptr = malloc(heap, size);
  if (new_ptr == nullptr) {
    return nullptr;
  }
  if (size > 0) {
    memcpy(new_ptr, ptr, size);
  }
  free(ptr);
  return new_ptr;
}

void free(void* ptr) {
  if (ptr == nullptr) {
    return;
  }

  auto* block_header = BlockFromDataPointer(ptr);
  if (block_header->kind == BlockKind::kLargeBlock) {
    auto* block = reinterpret_cast<LargeBlock*>(block_header);
    block->Free(ptr);
    heap_metadata->large_block_free_list.insert_back(*block);
  } else if (block_header->kind == BlockKind::kSmallBlock) {
    auto* block = reinterpret_cast<SmallBlock*>(block_header);
    block->Free(ptr);
    heap_metadata->small_block_free_list.EnsureContains(*block);
  }
}

}  // namespace jsmalloc

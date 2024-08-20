#include "src/jsmalloc/jsmalloc.h"

#include <cassert>
#include <cstddef>

#include "src/heap_interface.h"
#include "src/jsmalloc/allocator.h"
#include "src/jsmalloc/blocks/block.h"
#include "src/jsmalloc/blocks/free_block_allocator.h"
#include "src/jsmalloc/blocks/large_block_allocator.h"
#include "src/jsmalloc/blocks/sentinel_block_allocator.h"
#include "src/jsmalloc/blocks/small_block_allocator.h"
#include "src/jsmalloc/util/assert.h"
#include "src/jsmalloc/util/math.h"

namespace jsmalloc {

namespace {

class HeapGlobals {
 public:
  explicit HeapGlobals(bench::Heap& heap)
      : heap_allocator_(&heap),
        sentinel_block_allocator_(heap_allocator_),
        free_block_allocator_(sentinel_block_allocator_),
        large_block_allocator_(free_block_allocator_),
        small_block_allocator_(free_block_allocator_) {}

  void Start() {
    sentinel_block_allocator_.Start();
  }

  HeapAllocator heap_allocator_;
  blocks::SentinelBlockAllocator sentinel_block_allocator_;
  blocks::FreeBlockAllocator free_block_allocator_;
  blocks::LargeBlockAllocator large_block_allocator_;
  blocks::SmallBlockAllocator small_block_allocator_;
};

HeapGlobals* heap_globals = nullptr;

}  // namespace

void* sbrk_16b(bench::Heap& heap, size_t size) {
  DCHECK_EQ(size % 16, 0);
  return heap.sbrk(size);
}

// Called before any allocations are made.
void initialize_heap(bench::Heap& heap) {
  heap_globals = new (sbrk_16b(heap, math::round_16b(sizeof(HeapGlobals))))
      HeapGlobals(heap);
  heap_globals->Start();
}

void* malloc(size_t size) {
  if (size == 0) {
    return nullptr;
  }
  if (size <= blocks::SmallBlockAllocator::kMaxDataSize) {
    return heap_globals->small_block_allocator_.Allocate(size);
  }
  return heap_globals->large_block_allocator_.Allocate(size);
}

void* calloc(size_t nmemb, size_t size) {
  void* ptr = malloc(nmemb * size);
  if (ptr == nullptr) {
    return nullptr;
  }
  memset(ptr, 0, nmemb * size);
  return ptr;
}

void* realloc(void* ptr, size_t size) {
  void* new_ptr = malloc(size);
  if (new_ptr == nullptr) {
    return nullptr;
  }
  if (ptr != nullptr) {
    memcpy(new_ptr, ptr, size);
  }
  free(ptr);
  return new_ptr;
}

void free(void* ptr) {
  if (ptr == nullptr) {
    return;
  }
  blocks::BlockHeader* hdr = blocks::BlockHeader::FromDataPtr(ptr);
  if (hdr->Kind() == blocks::BlockKind::kLarge) {
    heap_globals->large_block_allocator_.Free(ptr);
  } else if (hdr->Kind() == blocks::BlockKind::kSmall) {
    heap_globals->small_block_allocator_.Free(ptr);
  }
}

}  // namespace jsmalloc

#include "src/jsmalloc/jsmalloc.h"

#include <cassert>
#include <cstddef>

#include "src/heap_factory.h"
#include "src/heap_interface.h"
#include "src/jsmalloc/allocator.h"
#include "src/jsmalloc/blocks/block.h"
#include "src/jsmalloc/blocks/free_block_allocator.h"
#include "src/jsmalloc/blocks/large_block_allocator.h"
#include "src/jsmalloc/blocks/sentinel_block_allocator.h"
#include "src/jsmalloc/blocks/small_block_allocator.h"

namespace jsmalloc {

namespace {

constexpr size_t kHeapSize = 512 << 20;

class FreeBlockHeap {
 public:
  explicit FreeBlockHeap(bench::Heap& heap)
      : heap_adaptor_(&heap),
        sentinel_block_heap_(heap_adaptor_),
        free_block_allocator_(sentinel_block_heap_) {}

  blocks::FreeBlockAllocator& FreeBlockAllocator() {
    return free_block_allocator_;
  }

  void Init() {
    sentinel_block_heap_.Init();
  }

  bool Contains(void* ptr) {
    return twiddle::PtrValue(ptr) >=
               twiddle::PtrValue(sentinel_block_heap_.Start()) &&
           twiddle::PtrValue(ptr) <
               twiddle::PtrValue(sentinel_block_heap_.End());
  }

 private:
  HeapAdaptor heap_adaptor_;
  blocks::SentinelBlockHeap sentinel_block_heap_;
  blocks::FreeBlockAllocator free_block_allocator_;
};

class HeapGlobals {
 public:
  explicit HeapGlobals(bench::HeapFactory& heap_factory,
                       bench::Heap& small_block_heap,
                       bench::Heap& large_block_heap)
      : heap_factory_(heap_factory),
        large_block_heap_(large_block_heap),
        large_block_allocator_(large_block_heap_.FreeBlockAllocator()),
        small_block_heap_(small_block_heap),
        small_block_allocator_(small_block_heap_.FreeBlockAllocator()) {}

  void Init() {
    large_block_heap_.Init();
    small_block_heap_.Init();
  }

  bench::HeapFactory& heap_factory_;
  FreeBlockHeap large_block_heap_;
  blocks::LargeBlockAllocator large_block_allocator_;
  FreeBlockHeap small_block_heap_;
  blocks::SmallBlockAllocator small_block_allocator_;
};

uint8_t globals_data[sizeof(HeapGlobals)];
HeapGlobals* heap_globals = reinterpret_cast<HeapGlobals*>(&globals_data);

}  // namespace

// Called before any allocations are made.
void initialize_heap(bench::HeapFactory& heap_factory) {
  HeapFactoryAdaptor mem_allocator(&heap_factory);

  auto large_block_heap = heap_factory.NewInstance(kHeapSize);
  if (!large_block_heap.ok()) {
    std::cerr << "Failed to initialize large block heap" << std::endl;
    std::exit(-1);
  }

  auto small_block_heap = heap_factory.NewInstance(kHeapSize);
  if (!small_block_heap.ok()) {
    std::cerr << "Failed to initialize small block heap" << std::endl;
    std::exit(-1);
  }

  heap_globals = new (globals_data) HeapGlobals(
      heap_factory, *small_block_heap->second, *large_block_heap->second);
  heap_globals->Init();
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

  if (heap_globals->small_block_heap_.Contains(ptr)) {
    heap_globals->small_block_allocator_.Free(ptr);
    return;
  }

  blocks::BlockHeader* hdr = blocks::BlockHeader::FromDataPtr(ptr);
  if (hdr->Kind() == blocks::BlockKind::kLarge) {
    heap_globals->large_block_allocator_.Free(ptr);
  } else {
    std::cerr << "unexpected block type: " << static_cast<int>(hdr->Kind())
              << std::endl;
    std::terminate();
  }
}

}  // namespace jsmalloc

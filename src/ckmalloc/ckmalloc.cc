#include "src/ckmalloc/ckmalloc.h"

#include <cstddef>
#include <cstring>
#include <optional>

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/global_state.h"
#include "src/ckmalloc/local_cache.h"
#include "src/ckmalloc/main_allocator.h"
#include "src/heap_factory.h"
#include "src/mmap_heap.h"

namespace ckmalloc {

using bench::MMapHeap;

namespace {

std::array<std::optional<bench::MMapHeap>, 2> g_heaps;

}

/* static */
CkMalloc* CkMalloc::instance_ = nullptr;

/* static */
void CkMalloc::Initialize() {
  LocalCache::ClearLocalCaches();
  auto h1 = MMapHeap::New(kHeapSize);
  auto h2 = MMapHeap::New(kHeapSize);
  g_heaps[0].emplace(std::move(h1.value()));
  g_heaps[1].emplace(std::move(h2.value()));
  InitializeWithEmptyHeaps(&g_heaps[0].value(), &g_heaps[1].value());
}

/* static */
void CkMalloc::InitializeHeap(bench::HeapFactory& heap_factory) {
  LocalCache::ClearLocalCaches();
  CK_ASSERT_EQ(heap_factory.Instance(0), nullptr);
  auto result = heap_factory.NewInstance(kHeapSize);
  CK_ASSERT_TRUE(result.ok());
  auto result2 = heap_factory.NewInstance(kHeapSize);
  CK_ASSERT_TRUE(result2.ok());
  InitializeWithEmptyHeaps(result.value().second, result2.value().second);
}

void* CkMalloc::Malloc(size_t size) {
  if (size == 0) {
    return nullptr;
  }

  LocalCache* cache = LocalCache::Instance<GlobalMetadataAlloc>();
  if (LocalCache::CanHoldSize(size)) {
    size_t alloc_size = AlignUserSize(size);
    void* cached_alloc = cache->TakeAlloc(alloc_size);
    if (cached_alloc != nullptr) {
      return cached_alloc;
    }
  }

  if (cache->ShouldFlush()) {
    cache->Flush(*global_state_.MainAllocator());
  }

  return global_state_.MainAllocator()->Alloc(size);
}

void* CkMalloc::Calloc(size_t nmemb, size_t size) {
  void* block = Malloc(nmemb * size);
  if (block != nullptr) {
    memset(block, 0, nmemb * size);
  }
  return block;
}

void* CkMalloc::Realloc(void* ptr, size_t size) {
  CK_ASSERT_NE(size, 0);
  if (ptr == nullptr) {
    return Malloc(size);
  }
  // TODO: use cache here.
  return global_state_.MainAllocator()->Realloc(ptr, size);
}

void CkMalloc::Free(void* ptr) {
  if (ptr == nullptr) {
    return;
  }

  MainAllocator* main_allocator = global_state_.MainAllocator();
  LocalCache* cache = LocalCache::Instance<GlobalMetadataAlloc>();
  size_t alloc_size = main_allocator->AllocSize(ptr);
  if (LocalCache::CanHoldSize(alloc_size)) {
    cache->CacheAlloc(ptr, alloc_size);
  } else {
    main_allocator->Free(ptr);
  }
}

size_t CkMalloc::GetSize(void* ptr) {
  return global_state_.MainAllocator()->AllocSize(ptr);
}

CkMalloc::CkMalloc(bench::Heap* meta_heap, bench::Heap* alloc_heap)
    : global_state_(meta_heap, alloc_heap) {}

/* static */
CkMalloc* CkMalloc::InitializeWithEmptyHeaps(bench::Heap* meta_heap,
                                             bench::Heap* alloc_heap) {
  // Allocate a metadata slab and place ourselves at the beginning of it.
  size_t metadata_size = AlignUp(sizeof(CkMalloc), kPageSize);
  void* metadata_heap_start = meta_heap->sbrk(metadata_size);
  CK_ASSERT_NE(metadata_heap_start, nullptr);

  CkMalloc* instance =
      new (metadata_heap_start) CkMalloc(meta_heap, alloc_heap);
  instance_ = instance;
  return instance;
}

Slab* GlobalMetadataAlloc::SlabAlloc() {
  return CkMalloc::Instance()->GlobalState()->MetadataManager()->NewSlabMeta();
}

void GlobalMetadataAlloc::SlabFree(MappedSlab* slab) {
  CkMalloc::Instance()->GlobalState()->MetadataManager()->FreeSlabMeta(slab);
}

void* GlobalMetadataAlloc::Alloc(size_t size, size_t alignment) {
  void* res = CkMalloc::Instance()->GlobalState()->MetadataManager()->Alloc(
      size, alignment);
  return res;
}

}  // namespace ckmalloc

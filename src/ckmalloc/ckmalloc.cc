#include "src/ckmalloc/ckmalloc.h"

#include <cstddef>
#include <cstring>

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/global_state.h"
#include "src/ckmalloc/local_cache.h"
#include "src/ckmalloc/main_allocator.h"
#include "src/ckmalloc/size_class.h"
#include "src/ckmalloc/util.h"
#include "src/heap_factory.h"
#include "src/heap_interface.h"

namespace ckmalloc {

/* static */
CkMalloc* CkMalloc::instance_ = nullptr;

/* static */
void CkMalloc::InitializeHeap(bench::HeapFactory& heap_factory) {
  LocalCache::ClearLocalCaches();
  InitializeWithEmptyHeap(&heap_factory);
}

void* CkMalloc::Malloc(size_t size) {
  if (size == 0) {
    return nullptr;
  }

  LocalCache* cache = LocalCache::Instance<GlobalMetadataAlloc>();
  if (LocalCache::CanHoldSize(size)) {
    size_t alloc_size = SizeClass::FromUserDataSize(size).SliceSize();
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
  Void* p = reinterpret_cast<Void*>(ptr);
  if (p == nullptr) {
    return Malloc(size);
  }
  // TODO: use cache here.
  return global_state_.MainAllocator()->Realloc(p, size);
}

void CkMalloc::Free(void* ptr) {
  Void* p = reinterpret_cast<Void*>(ptr);
  if (p == nullptr) {
    return;
  }

  MainAllocator* main_allocator = global_state_.MainAllocator();
  LocalCache* cache = LocalCache::Instance<GlobalMetadataAlloc>();
  size_t alloc_size = main_allocator->AllocSize(p);
  if (LocalCache::CanHoldSize(alloc_size)) {
    cache->CacheAlloc(p, alloc_size);
  } else {
    main_allocator->Free(p);
  }
}

size_t CkMalloc::GetSize(void* ptr) {
  return global_state_.MainAllocator()->AllocSize(reinterpret_cast<Void*>(ptr));
}

CkMalloc::CkMalloc(bench::Heap* metadata_heap, bench::Heap* user_heap)
    : global_state_(metadata_heap, user_heap) {}

/* static */
CkMalloc* CkMalloc::InitializeWithEmptyHeap(bench::HeapFactory* heap_factory) {
  CK_ASSERT_TRUE(heap_factory->Instances().empty());
  auto result = heap_factory->NewInstance(kHeapSize);
  CK_ASSERT_TRUE(result.ok());
  auto result2 = heap_factory->NewInstance(kHeapSize);
  CK_ASSERT_TRUE(result2.ok());

  // Allocate a metadata slab and place ourselves at the beginning of it.
  size_t metadata_size = AlignUp(sizeof(CkMalloc), kPageSize);
  void* metadata_heap_start = result.value()->sbrk(metadata_size);
  CK_ASSERT_NE(metadata_heap_start, nullptr);

  CkMalloc* instance =
      new (metadata_heap_start) CkMalloc(result.value(), result2.value());
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

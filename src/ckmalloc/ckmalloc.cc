#include "src/ckmalloc/ckmalloc.h"

#include <cstddef>
#include <cstring>

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/global_state.h"
#include "src/ckmalloc/local_cache.h"
#include "src/ckmalloc/main_allocator.h"
#include "src/ckmalloc/size_class.h"
#include "src/ckmalloc/sys_alloc.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

/* static */
CkMalloc* CkMalloc::instance_ = nullptr;

/* static */
void CkMalloc::InitializeHeap() {
  LocalCache::ClearLocalCaches();
  Initialize();
}

void* CkMalloc::Malloc(size_t size, size_t alignment) {
  (void) alignment;
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
  void* block = Malloc(nmemb * size, /*alignment=*/0);
  if (block != nullptr) {
    memset(block, 0, nmemb * size);
  }
  return block;
}

void* CkMalloc::Realloc(void* ptr, size_t size) {
  CK_ASSERT_NE(size, 0);
  Void* p = reinterpret_cast<Void*>(ptr);
  if (p == nullptr) {
    return Malloc(size, /*alignment=*/0);
  }
  // TODO: use cache here.
  return global_state_.MainAllocator()->Realloc(p, size);
}

void CkMalloc::Free(void* ptr, size_t size_hint, size_t alignment_hint) {
  (void) size_hint;
  (void) alignment_hint;
  Void* p = reinterpret_cast<Void*>(ptr);
  if (p == nullptr) {
    return;
  }

  MainAllocator* main_allocator = global_state_.MainAllocator();
  LocalCache* cache = LocalCache::Instance<GlobalMetadataAlloc>();
  SizeClass size_class = main_allocator->AllocSizeClass(p);
  if (size_class != SizeClass::Nil() &&
      LocalCache::CanHoldSize(size_class.SliceSize())) {
    cache->CacheAlloc(p, size_class.SliceSize());
  } else {
    main_allocator->Free(p);
  }
}

size_t CkMalloc::GetSize(void* ptr) {
  return global_state_.MainAllocator()->AllocSize(reinterpret_cast<Void*>(ptr));
}

CkMalloc::CkMalloc(void* metadata_heap, void* metadata_heap_end,
                   void* user_heap)
    : global_state_(metadata_heap, metadata_heap_end, user_heap) {}

/* static */
CkMalloc* CkMalloc::Initialize() {
  SysAlloc* alloc = SysAlloc::Instance();
  CK_ASSERT_NE(alloc, nullptr);
  void* metadata_heap = alloc->Mmap(/*start_hint=*/nullptr, kHeapSize);
  void* user_heap = alloc->Mmap(/*start_hint=*/nullptr, kHeapSize);

  // Allocate a metadata slab and place ourselves at the beginning of it.
  alloc->Sbrk(metadata_heap, sizeof(CkMalloc), metadata_heap);
  void* metadata_heap_end = PtrAdd(metadata_heap, sizeof(CkMalloc));

  CkMalloc* instance =
      new (metadata_heap) CkMalloc(metadata_heap, metadata_heap_end, user_heap);
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

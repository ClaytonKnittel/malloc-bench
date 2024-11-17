#pragma once

#include <cstdint>

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/size_class.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

template <MainAllocatorInterface MainAllocator>
class CacheCleanup {
 public:
  ~CacheCleanup();

  static CacheCleanup& Instance() {
    return cache_cleanup_;
  }

  static void Destroy() {
    cache_cleanup_.~CacheCleanup();
  }

  void RegisterCleanup(MainAllocator* main_allocator) {
    main_allocator_ = main_allocator;
  }

 private:
  static CK_CONST_INIT thread_local CacheCleanup cache_cleanup_ CK_INITIAL_EXEC;

  MainAllocator* main_allocator_ = nullptr;
};

template <MainAllocatorInterface MainAllocator>
/* static */
CK_CONST_INIT thread_local CacheCleanup<MainAllocator>
    CacheCleanup<MainAllocator>::cache_cleanup_ CK_INITIAL_EXEC =
        CacheCleanup();

class LocalCache {
  template <MainAllocatorInterface>
  friend class CacheCleanup;
  friend class TraceReplayer;

 public:
  LocalCache() = default;

  static LocalCache* Instance();

  template <MetadataAllocInterface MetadataAlloc,
            MainAllocatorInterface MainAllocator>
  static LocalCache* InstanceOrInitialize(MainAllocator& main_allocator);

  template <MainAllocatorInterface MainAllocator>
  static void ClearLocalCache();

  // Takes and returns an allocation of the given size from the cache, if one
  // exists, otherwise returning `nullptr`.
  Void* TakeAlloc(SizeClass size_class);

  // Caches an allocation with user-allocatable memory beginning at `ptr` of the
  // given size `alloc_size`.
  void CacheAlloc(Void* ptr, SizeClass size_class);

  // If true, the cache recommends flushing before the next allocation to avoid
  // excessive memory fragmentation.
  bool ShouldFlush() const;

  template <MainAllocatorInterface MainAllocator>
  void Flush(MainAllocator& main_allocator);

  static bool CanHoldSize(size_t alloc_size) {
    return alloc_size <= kMaxSmallSize;
  }

 private:
  // Cached allocs are previously freed allocations which have not yet been
  // given back to the main allocator, assuming that more allocations of this
  // size will be made soon.
  struct CachedAlloc {
    CachedAlloc* next;
  };
  // All allocations must be able to hold `CachedAlloc`, so this struct may be
  // no larger than the smallest possible allocation.
  static_assert(sizeof(CachedAlloc) <= kMinAlignment,
                "CachedAlloc is larger than the smallest possible allocation");

  // Once the cache exceeds this size, it is flushed after the next allocation.
  static constexpr uint32_t kMaxCacheSize = 128;

  // Thread-local instances of the cache.
  static CK_CONST_INIT thread_local LocalCache* instance_ CK_INITIAL_EXEC;

  // The bins are singly-linked lists of allocations ready to hand out (i.e. the
  // main allocator views them as allocated) of a particular size.
  // TODO: Try a skiplist here too for clearing?
  CachedAlloc* bins_[SizeClass::kNumSizeClasses] = {};

  // The count of allocs held in the cache.
  uint32_t total_allocs_ = 0;
};

template <MainAllocatorInterface MainAllocator>
CacheCleanup<MainAllocator>::~CacheCleanup() {
  if (main_allocator_ != nullptr) {
    LocalCache::instance_->Flush(*main_allocator_);
    main_allocator_ = nullptr;
  }
}

/* static */
template <MetadataAllocInterface MetadataAlloc,
          MainAllocatorInterface MainAllocator>
LocalCache* LocalCache::InstanceOrInitialize(MainAllocator& main_allocator) {
  if (CK_EXPECT_FALSE(instance_ == nullptr)) {
    void* instance_data =
        (MetadataAlloc::Alloc(sizeof(LocalCache), alignof(LocalCache)));
    instance_ = new (instance_data) LocalCache();
    CK_ASSERT_NE(instance_, nullptr);

    CacheCleanup<MainAllocator>::Instance().RegisterCleanup(&main_allocator);
  }

  return instance_;
}

/* static */
template <MainAllocatorInterface MainAllocator>
void LocalCache::ClearLocalCache() {
  CacheCleanup<MainAllocator>::Destroy();
  instance_ = nullptr;
}

template <MainAllocatorInterface MainAllocator>
void LocalCache::Flush(MainAllocator& main_allocator) {
  for (CachedAlloc*& bin : bins_) {
    for (CachedAlloc* alloc = bin; alloc != nullptr;) {
      CachedAlloc* next_alloc = alloc->next;
      main_allocator.Free(reinterpret_cast<Void*>(alloc));
      alloc = next_alloc;
    }
    bin = nullptr;
    total_allocs_ = 0;
  }
}

}  // namespace ckmalloc

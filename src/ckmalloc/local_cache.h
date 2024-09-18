#pragma once

#include <cstddef>
#include <cstdint>

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

inline constexpr size_t ComputeSizeIdx(size_t alloc_size) {
  return alloc_size / kDefaultAlignment + (alloc_size > kMaxSmallSize ? 1 : 0);
}

class LocalCache {
  friend class TraceReplayer;

 public:
  LocalCache() = default;

  template <MetadataAllocInterface MetadataAlloc>
  static LocalCache* Instance();

  static void ClearLocalCaches();

  // Takes and returns an allocation of the given size from the cache, if one
  // exists, otherwise returning `nullptr`.
  Void* TakeAlloc(size_t alloc_size);

  // Caches an allocation with user-allocatable memory beginning at `ptr` of the
  // given size `alloc_size`.
  void CacheAlloc(Void* ptr, size_t alloc_size);

  // If true, the cache recommends flushing before the next allocation to avoid
  // excessive memory fragmentation.
  bool ShouldFlush() const;

  template <MainAllocatorInterface MainAllocator>
  void Flush(MainAllocator& main_allocator);

  static bool CanHoldSize(size_t alloc_size) {
    return alloc_size <= kMaxCachedAllocSize;
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

  // Returns the index into the bins list of an allocation of the given size.
  static size_t SizeIdx(size_t alloc_size);

  static constexpr size_t kMaxCachedAllocSize = 128;
  static constexpr size_t kNumCacheBins =
      ComputeSizeIdx(kMaxCachedAllocSize) + 1;

  // Once the cache exceeds this size, it is flushed after the next allocation.
  static constexpr uint32_t kMaxCacheSize = 128;

  static LocalCache* instance_;

  // The bins are singly-linked lists of allocations ready to hand out (i.e. the
  // main allocator views them as allocated) of a particular size.
  // TODO: Try a skiplist here too for clearing?
  CachedAlloc* bins_[kNumCacheBins] = {};

  // The count of allocs held in the cache.
  uint32_t total_allocs_ = 0;
};

/* static */
template <MetadataAllocInterface MetadataAlloc>
LocalCache* LocalCache::Instance() {
  if (CK_EXPECT_FALSE(instance_ == nullptr)) {
    void* instance_data =
        (MetadataAlloc::Alloc(sizeof(LocalCache), alignof(LocalCache)));
    instance_ = new (instance_data) LocalCache();
    CK_ASSERT_NE(instance_, nullptr);
  }

  return instance_;
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

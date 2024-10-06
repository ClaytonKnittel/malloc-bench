#include "src/ckmalloc/local_cache.h"

#include "src/ckmalloc/common.h"

namespace ckmalloc {

/* static */
LocalCache* LocalCache::instance_ = nullptr;

/* static */
void LocalCache::ClearLocalCaches() {
  instance_ = nullptr;
}

Void* LocalCache::TakeAlloc(SizeClass size_class) {
  CachedAlloc* top = bins_[size_class.Ordinal()];
  if (top == nullptr) {
    return nullptr;
  }

  bins_[size_class.Ordinal()] = top->next;
  total_allocs_--;
  return reinterpret_cast<Void*>(top);
}

void LocalCache::CacheAlloc(Void* ptr, SizeClass size_class) {
  CachedAlloc* alloc = reinterpret_cast<CachedAlloc*>(ptr);
  alloc->next = bins_[size_class.Ordinal()];
  bins_[size_class.Ordinal()] = alloc;
  total_allocs_++;
}

bool LocalCache::ShouldFlush() const {
  return total_allocs_ >= kMaxCacheSize;
}

}  // namespace ckmalloc

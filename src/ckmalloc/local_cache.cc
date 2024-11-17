#include "src/ckmalloc/local_cache.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

/* static */
CK_CONST_INIT thread_local LocalCache* LocalCache::instance_ CK_INITIAL_EXEC =
    nullptr;

/* static */
LocalCache* LocalCache::Instance() {
  return instance_;
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

#include "src/ckmalloc/local_cache.h"

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/common.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

/* static */
LocalCache* LocalCache::instance_ = nullptr;

/* static */
void LocalCache::ClearLocalCaches() {
  instance_ = nullptr;
}

void* LocalCache::TakeAlloc(size_t alloc_size) {
  size_t idx = SizeIdx(alloc_size);
  CachedAlloc* top = bins_[idx];
  if (top == nullptr) {
    return nullptr;
  }

  bins_[idx] = top->next;
  total_allocs_--;
  return top;
}

void LocalCache::CacheAlloc(void* ptr, size_t alloc_size) {
  size_t idx = SizeIdx(alloc_size);
  CachedAlloc* alloc = reinterpret_cast<CachedAlloc*>(ptr);
  alloc->next = bins_[idx];
  bins_[idx] = alloc;
  total_allocs_++;
}

bool LocalCache::ShouldFlush() const {
  return total_allocs_ >= kMaxCacheSize;
}

/* static */
size_t LocalCache::SizeIdx(size_t alloc_size) {
  CK_ASSERT_TRUE(CanHoldSize(alloc_size));
  CK_ASSERT_TRUE(
      alloc_size == kMinAlignment ||
      (alloc_size <= kMaxSmallSize &&
       IsAligned(alloc_size, kDefaultAlignment)) ||
      (alloc_size > kMaxSmallSize &&
       IsAligned(Block::kMetadataOverhead + alloc_size, kDefaultAlignment)));
  return alloc_size / kDefaultAlignment;
}

}  // namespace ckmalloc

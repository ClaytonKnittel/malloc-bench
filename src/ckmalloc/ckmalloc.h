#pragma once

#include <atomic>
#include <cstddef>

#include "absl/synchronization/mutex.h"

#include "src/ckmalloc/global_state.h"
#include "src/ckmalloc/local_cache.h"
#include "src/ckmalloc/main_allocator.h"
#include "src/ckmalloc/sys_alloc.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

class CkMalloc {
 public:
  // Returns the singleton `CkMalloc` instance.
  static CkMalloc* Instance() {
    CkMalloc* instance = instance_.load(std::memory_order_acquire);
    if (CK_EXPECT_FALSE(instance == nullptr)) {
      absl::MutexLock lock(&mutex_);
      instance = instance_.load(std::memory_order_acquire);
      if (instance == nullptr) {
        RealSysAlloc::UseRealSysAlloc();
        instance = InitializeHeap();
      }
    }
    CK_ASSERT_NE(instance, nullptr);
    instance->global_state_.AssertConsistency();
    return instance;
  }

  static void Reset() {
    LocalCache::ClearLocalCache<MainAllocator>();
    instance_.store(nullptr, std::memory_order_relaxed);
  }

  static CkMalloc* InitializeHeap();

  void* Malloc(size_t size, size_t alignment);

  void* Calloc(size_t nmemb, size_t size);

  void* Realloc(void* ptr, size_t size);

  void Free(void* ptr, size_t size_hint, size_t alignment_hint);

  size_t GetSize(void* ptr);

  GlobalState* GlobalState() {
    return &global_state_;
  }

 private:
  explicit CkMalloc(void* metadata_heap, void* metadata_heap_end);

  // Initializes the allocator by allocating the metadata heap and first user
  // heap.
  static CkMalloc* Initialize();

  static std::atomic<CkMalloc*> instance_;

  class GlobalState global_state_;

  static absl::Mutex mutex_;
};

}  // namespace ckmalloc

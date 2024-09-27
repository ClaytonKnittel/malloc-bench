#pragma once

#include <cstddef>

#include "src/ckmalloc/global_state.h"

namespace ckmalloc {

class CkMalloc {
 public:
  // Returns the singleton `CkMalloc` instance.
  static CkMalloc* Instance() {
    CK_ASSERT_NE(instance_, nullptr);
    instance_->global_state_.AssertConsistency();
    return instance_;
  }

  static void InitializeHeap();

  void* Malloc(size_t size, size_t alignment);

  void* Calloc(size_t nmemb, size_t size);

  void* Realloc(void* ptr, size_t size);

  void Free(void* ptr, size_t size_hint, size_t alignment_hint);

  size_t GetSize(void* ptr);

  GlobalState* GlobalState() {
    return &global_state_;
  }

 private:
  explicit CkMalloc(void* metadata_heap, void* metadata_heap_end,
                    void* user_heap);

  // Initializes the allocator by allocating the metadata heap and first user
  // heap.
  static CkMalloc* Initialize();

  static CkMalloc* instance_;

  class GlobalState global_state_;
};

}  // namespace ckmalloc

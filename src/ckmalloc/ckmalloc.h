#pragma once

#include <cstddef>

#include "src/ckmalloc/state.h"
#include "src/heap_factory.h"

namespace ckmalloc {

class CkMalloc {
 public:
  // Returns the singleton `CkMalloc` instance.
  static CkMalloc* Instance() {
    CK_ASSERT_NE(instance_, nullptr);
    instance_->global_state_.AssertConsistency();
    return instance_;
  }

  static void InitializeHeap(bench::HeapFactory& heap_factory);

  void* Malloc(size_t size);

  void* Calloc(size_t nmemb, size_t size);

  void* Realloc(void* ptr, size_t size);

  void Free(void* ptr);

  GlobalState* GlobalState() {
    return &global_state_;
  }

 private:
  explicit CkMalloc(bench::HeapFactory* heap_factory);

  // Initializes a new `CkMalloc` with a heap factory that has not been
  // allocated from yet. The `CkMalloc` takes ownership of the heap factory.
  static CkMalloc* InitializeWithEmptyHeap(bench::HeapFactory* heap_factory);

  static CkMalloc* instance_;

  class GlobalState global_state_;
};

}  // namespace ckmalloc

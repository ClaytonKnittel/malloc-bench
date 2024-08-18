#pragma once

#include <cstddef>

#include "absl/status/statusor.h"

#include "src/heap_interface.h"

namespace bench {

class SingletonHeap : public Heap {
 public:
  // Max heap size is 512 MB.
  static constexpr size_t kHeapSize = 512 * (1 << 20);
  // static constexpr size_t kHeapSize = 2048LU * (1 << 20);

  SingletonHeap(SingletonHeap&&) = default;
  ~SingletonHeap();

  // Returns the singleton global heap instance, initializing it if it does not
  // yet exist.
  static SingletonHeap* GlobalInstance();

 private:
  SingletonHeap(void* heap_start, size_t size);

  static absl::StatusOr<SingletonHeap> Initialize();

  // The global heap instance.
  static SingletonHeap global_heap_;
};

}  // namespace bench

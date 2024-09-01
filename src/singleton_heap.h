#pragma once

#include <cstddef>

#include "src/heap_interface.h"

namespace bench {

class SingletonHeap {
  friend class CorrectnessChecker;

 public:
  // Max heap size is 512 MB.
  static constexpr size_t kHeapSize = 512 * (1 << 20);

  // Returns the singleton global heap instance, initializing it if it does not
  // yet exist.
  static Heap* GlobalInstance();

  static void Reset();
};

}  // namespace bench

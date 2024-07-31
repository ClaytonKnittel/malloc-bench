#pragma once

#include "src/heap_interface.h"

namespace bench::test {

// A simulated heap which does not actually allocate memory, but may be
// `sbrk`ed. Used for testing purposes only.
class SimHeap : public Heap {
 public:
  SimHeap(void* heap_start, size_t size) : Heap(heap_start, size) {}
};

}  // namespace bench::test

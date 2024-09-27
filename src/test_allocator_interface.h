#pragma once

#include "src/ckmalloc/ckmalloc.h"
#include "src/ckmalloc/testlib.h"
#include "src/heap_factory.h"

namespace bench {

// Called before any allocations are made.
inline void initialize_test_heap(HeapFactory& heap_factory) {
  ckmalloc::TestSysAlloc::NewInstance(&heap_factory);
  ckmalloc::CkMalloc::InitializeHeap();
}

}  // namespace bench
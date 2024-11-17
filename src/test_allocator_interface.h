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

// Called after a trace run to clear all internal data structs before another
// run.
inline void reset_test_heap() {
  ckmalloc::CkMalloc::Reset();
  ckmalloc::TestSysAlloc::Reset();
}

}  // namespace bench
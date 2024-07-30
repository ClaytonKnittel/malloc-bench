#include "src/ckmalloc/fake_allocator.h"

#include "src/ckmalloc/util.h"
#include "src/fake_heap.h"

namespace ckmalloc {

FakeAllocator::FakeAllocator()
    : region_start_(bench::FakeHeap::GlobalInstance()->Start()),
      region_end_(bench::FakeHeap::GlobalInstance()->End()) {
  CK_ASSERT(region_start_ == region_end_);
}

}  // namespace ckmalloc

#include "src/ckmalloc/testlib.h"

#include <new>
#include <vector>

#include "src/ckmalloc/slab.h"

namespace ckmalloc {

namespace {

// std::vector<void*> allocs;

}

Slab* TestGlobalMetadataAlloc::SlabAlloc() {
#ifdef __cpp_aligned_new
  return reinterpret_cast<Slab*>(::operator new(
      sizeof(Slab), static_cast<std::align_val_t>(alignof(Slab))));
#else
  return reinterpret_cast<Slab*>(::operator new(sizeof(Slab)));
#endif
}

void TestGlobalMetadataAlloc::SlabFree(Slab* slab) {
#ifdef __cpp_aligned_new
  ::operator delete(slab, static_cast<std::align_val_t>(alignof(Slab)));
#else
  ::operator delete(slab);
#endif
}

void* TestGlobalMetadataAlloc::Alloc(size_t size, size_t alignment) {
#ifdef __cpp_aligned_new
  return ::operator new(size, static_cast<std::align_val_t>(alignment));
#else
  (void) alignment;
  return ::operator new(size);
#endif
}

}  // namespace ckmalloc

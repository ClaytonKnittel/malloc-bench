#include "src/ckmalloc/testlib.h"

#include <new>
#include <vector>

#include "src/ckmalloc/slab.h"

namespace ckmalloc {

namespace {

std::vector<std::pair<void*, std::align_val_t>> allocs;

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
  auto align_val = static_cast<std::align_val_t>(alignment);
#ifdef __cpp_aligned_new
  void* ptr = ::operator new(size, align_val);
#else
  void* ptr = ::operator new(size);
#endif

  allocs.emplace_back(ptr, align_val);
  return ptr;
}

void TestGlobalMetadataAlloc::ClearAllAllocs() {
  for (auto [ptr, align_val] : allocs) {
#ifdef __cpp_aligned_new
    ::operator delete(ptr, align_val);
#else
    ::operator delete(ptr);
#endif
  }
  allocs.clear();
}

}  // namespace ckmalloc

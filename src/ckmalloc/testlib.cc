#include "src/ckmalloc/testlib.h"

#include <memory>
#include <new>
#include <vector>

#include "absl/status/statusor.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

namespace {

std::vector<std::pair<void*, std::align_val_t>> allocs;

DetachedMetadataAlloc default_detached_allocator;

}  // namespace

TestMetadataAllocInterface* TestGlobalMetadataAlloc::allocator_ =
    &default_detached_allocator;

Slab* DetachedMetadataAlloc::SlabAlloc() {
#ifdef __cpp_aligned_new
  return reinterpret_cast<Slab*>(::operator new(
      sizeof(Slab), static_cast<std::align_val_t>(alignof(Slab))));
#else
  return reinterpret_cast<Slab*>(::operator new(sizeof(Slab)));
#endif
}

void DetachedMetadataAlloc::SlabFree(MappedSlab* slab) {
#ifdef __cpp_aligned_new
  ::operator delete(slab, static_cast<std::align_val_t>(alignof(Slab)));
#else
  ::operator delete(slab);
#endif
}

void* DetachedMetadataAlloc::Alloc(size_t size, size_t alignment) {
  auto align_val = static_cast<std::align_val_t>(alignment);
#ifdef __cpp_aligned_new
  void* ptr = ::operator new(size, align_val);
#else
  void* ptr = ::operator new(size);
#endif

  allocs.emplace_back(ptr, align_val);
  return ptr;
}

void DetachedMetadataAlloc::ClearAllAllocs() {
  for (auto [ptr, align_val] : allocs) {
#ifdef __cpp_aligned_new
    ::operator delete(ptr, align_val);
#else
    ::operator delete(ptr);
#endif
  }
  allocs.clear();
}

Slab* TestGlobalMetadataAlloc::SlabAlloc() {
  return allocator_->SlabAlloc();
}

void TestGlobalMetadataAlloc::SlabFree(MappedSlab* slab) {
  allocator_->SlabFree(slab);
}

void* TestGlobalMetadataAlloc::Alloc(size_t size, size_t alignment) {
  return allocator_->Alloc(size, alignment);
}

void TestGlobalMetadataAlloc::ClearAllAllocs() {
  allocator_->ClearAllAllocs();
}

/* static */
void TestGlobalMetadataAlloc::OverrideAllocator(
    TestMetadataAllocInterface* allocator) {
  CK_ASSERT_EQ(allocator_, &default_detached_allocator);
  allocator_ = allocator;
}

/* static */
void TestGlobalMetadataAlloc::ClearAllocatorOverride() {
  allocator_ = &default_detached_allocator;
}

absl::StatusOr<std::unique_ptr<bench::Heap>> TestHeapFactory::MakeHeap(
    size_t size) {
  CK_ASSERT_TRUE(IsAligned(size, kPageSize));
  return std::make_unique<TestHeap>(size / kPageSize);
}

}  // namespace ckmalloc

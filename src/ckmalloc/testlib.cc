#include "src/ckmalloc/testlib.h"

#include <cstdint>
#include <memory>
#include <new>
#include <vector>

#include "absl/status/statusor.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/sys_alloc.h"
#include "src/ckmalloc/util.h"
#include "src/heap_interface.h"

namespace ckmalloc {

namespace {

std::vector<std::pair<void*, std::align_val_t>> allocs;

DetachedMetadataAlloc default_detached_allocator;

}  // namespace

TestMetadataAllocInterface* TestGlobalMetadataAlloc::allocator_ =
    &default_detached_allocator;

uint64_t TestGlobalMetadataAlloc::n_allocs_ = 0;

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

/* static */
Slab* TestGlobalMetadataAlloc::SlabAlloc() {
  return allocator_->SlabAlloc();
}

/* static */
void TestGlobalMetadataAlloc::SlabFree(MappedSlab* slab) {
  allocator_->SlabFree(slab);
}

/* static */
void* TestGlobalMetadataAlloc::Alloc(size_t size, size_t alignment) {
  n_allocs_++;
  return allocator_->Alloc(size, alignment);
}

/* static */
uint64_t TestGlobalMetadataAlloc::TotalAllocs() {
  return n_allocs_;
}

/* static */
void TestGlobalMetadataAlloc::ClearAllAllocs() {
  allocator_->ClearAllAllocs();
  n_allocs_ = 0;
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

TestHeap* RandomHeapFromFactory(bench::HeapFactory& heap_factory) {
  return heap_factory.WithInstances<TestHeap*>(
      [](const auto& instances) -> TestHeap* {
        return static_cast<TestHeap*>(instances.begin()->get());
      });
}

TestSysAlloc::TestSysAlloc(bench::HeapFactory* heap_factory)
    : heap_factory_(heap_factory) {
  heap_factory_->WithInstances<void>([this](const auto& instances) {
    for (const auto& heap : instances) {
      // Assume all already-created heaps are metadata heaps.
      heap_map_.emplace(heap->Start(),
                        std::make_pair(HeapType::kMetadataHeap, heap.get()));
    }
  });
}

/* static */
TestSysAlloc* TestSysAlloc::Instance() {
  return static_cast<TestSysAlloc*>(instance_);
}

/* static */
TestSysAlloc* TestSysAlloc::NewInstance(bench::HeapFactory* heap_factory) {
  TestSysAlloc* sys_alloc = new TestSysAlloc(heap_factory);
  CK_ASSERT_EQ(instance_, nullptr);
  instance_ = sys_alloc;
  return sys_alloc;
}

/* static */
void TestSysAlloc::Reset() {
  delete instance_;
  instance_ = nullptr;
}

void* TestSysAlloc::Mmap(void* start_hint, size_t size, HeapType type) {
  (void) start_hint;
  auto result = heap_factory_->NewInstance(size);
  if (!result.ok()) {
    std::cerr << "Mmap failed: " << result.status() << std::endl;
    return nullptr;
  }

  bench::Heap* heap = result.value();
  void* heap_start = heap->Start();
  heap_map_.emplace(heap_start, std::make_pair(type, heap));

  return heap_start;
}

void TestSysAlloc::Munmap(void* ptr, size_t size) {
  auto it = heap_map_.find(ptr);
  CK_ASSERT_TRUE(it != heap_map_.end());
  bench::Heap* heap = it->second.second;
  CK_ASSERT_EQ(size, heap->MaxSize());

  auto result = heap_factory_->DeleteInstance(heap);
  if (!result.ok()) {
    std::cerr << "Heap delete failed: " << result << std::endl;
    CK_ASSERT_TRUE(result.ok());
  }

  heap_map_.erase(it);
}

void TestSysAlloc::Sbrk(void* heap_start, size_t increment, void* current_end) {
  bench::Heap* heap = HeapFromStart(heap_start);

  void* result = heap->sbrk(increment);
  CK_ASSERT_EQ(result, current_end);
}

bench::Heap* TestSysAlloc::HeapFromStart(void* heap_start) {
  auto it = heap_map_.find(heap_start);
  CK_ASSERT_TRUE(it != heap_map_.end());
  return it->second.second;
}

size_t TestSysAlloc::Size() const {
  return heap_map_.size();
}

TestSysAlloc::const_iterator TestSysAlloc::begin() const {
  return heap_map_.begin();
}

TestSysAlloc::const_iterator TestSysAlloc::end() const {
  return heap_map_.end();
}

}  // namespace ckmalloc

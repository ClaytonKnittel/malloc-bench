#include "src/ckmalloc/allocator.h"

#include <cstddef>
#include <new>

#include "gtest/gtest.h"

#include "src/ckmalloc/util.h"
#include "src/sim_heap.h"

namespace ckmalloc {

class AllocatorTest : public ::testing::Test {
 protected:
  AllocatorTest() = default;
  ~AllocatorTest() override {
    delete alloc_;
    delete heap_;
  }

  Allocator& MakeAllocator(size_t size) {
    CK_ASSERT(alloc_ == nullptr);
    heap_ = new bench::test::SimHeap(heap_start_, size);
    alloc_ = new Allocator(heap_);
    return *alloc_;
  }

  bench::Heap& Heap() {
    return *heap_;
  }

  const void* PtrAt(ptrdiff_t offset) const {
    return static_cast<uint8_t*>(heap_start_) + offset;
  }

  void* const heap_start_ = reinterpret_cast<void*>(0x1000);
  bench::test::SimHeap* heap_ = nullptr;
  Allocator* alloc_ = nullptr;
};

TEST_F(AllocatorTest, BeginsEmpty) {
  MakeAllocator(100);
  EXPECT_EQ(Heap().Size(), 0);
  EXPECT_EQ(Heap().Start(), heap_start_);
  EXPECT_EQ(Heap().End(), heap_start_);
}

TEST_F(AllocatorTest, InsertReturnsHeapStart) {
  Allocator& alloc = MakeAllocator(100);

  EXPECT_EQ(alloc.Alloc(10), PtrAt(0));
}

TEST_F(AllocatorTest, SecondInsertAfterFirst) {
  Allocator& alloc = MakeAllocator(100);

  alloc.Alloc(10);
  EXPECT_EQ(alloc.Alloc(5), PtrAt(10));
}

TEST_F(AllocatorTest, SecondInsertAligned) {
  Allocator& alloc = MakeAllocator(100);

  alloc.Alloc(10);
  EXPECT_EQ(alloc.Alloc(8, std::align_val_t(8)), PtrAt(16));
}

}  // namespace ckmalloc

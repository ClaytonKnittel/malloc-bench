#pragma once

#include <cstdint>

#include "src/heap_factory.h"
#include "src/heap_interface.h"
#include "src/jsmalloc/util/assert.h"
#include "src/jsmalloc/util/twiddle.h"

namespace jsmalloc {

class MemRegion {
 public:
  /** Extends a memory region by the provided amount. */
  virtual void* Extend(intptr_t increment) = 0;
  virtual void* Start() = 0;
  virtual void* End() = 0;
  virtual ~MemRegion() = default;
};

class MemRegionAllocator {
 public:
  /** Returns a pointer to a new memory region. */
  virtual MemRegion* New(size_t max_size) = 0;
  virtual ~MemRegionAllocator() = default;
};

class HeapAdaptor : public MemRegion {
 public:
  explicit HeapAdaptor(bench::Heap* heap) : heap_(heap){};

  void* Extend(intptr_t increment) override {
    return heap_->sbrk(increment);
  }

  void* Start() override {
    return heap_->Start();
  }

  void* End() override {
    return heap_->End();
  }

  bool Contains(void* ptr) {
    return twiddle::PtrValue(ptr) >= twiddle::PtrValue(heap_->Start()) &&
           twiddle::PtrValue(ptr) < twiddle::PtrValue(heap_->End());
  }

 private:
  bench::Heap* heap_;
};

class HeapFactoryAdaptor : public MemRegionAllocator {
 public:
  explicit HeapFactoryAdaptor(bench::HeapFactory* heap_factory)
      : heap_factory_(heap_factory){};

  MemRegion* New(size_t size) override {
    auto s = heap_factory_->NewInstance(size);
    if (!s.ok()) {
      return nullptr;
    }
    regions_.emplace_back(*s);
    return &regions_.back();
  }

 private:
  bench::HeapFactory* heap_factory_;
  std::vector<HeapAdaptor> regions_;
};

namespace testing {

/**
 * Allocator for testing.
 */
template <size_t N>
class FixedSizeTestHeap : public MemRegion {
 public:
  void* Extend(intptr_t increment) override {
    DCHECK_EQ(increment % 16, 0);
    DCHECK_LE(increment, N);
    if (end_ + increment >= N) {
      return nullptr;
    }
    // Ensure we give out 16-byte aligned addresses.
    void* ptr = End();
    end_ += increment;
    return ptr;
  }

  void* Start() override {
    return &data_[Offset()];
  }

  void* End() override {
    return &data_[Offset() + end_];
  }

 private:
  intptr_t Offset() {
    return 16 - (twiddle::PtrValue(this) % 16);
  }

  size_t end_ = 0;
  uint8_t data_[N];
};

using TestHeap = FixedSizeTestHeap<1 << 20>;

}  // namespace testing

}  // namespace jsmalloc

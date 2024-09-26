#pragma once

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_map.h"
#include "src/heap_interface.h"

namespace ckmalloc {

template <SlabMapInterface SlabMap>
class HeapIteratorImpl {
 public:
  HeapIteratorImpl(const SlabMap* slab_map, PageId page_id)
      : slab_map_(slab_map), current_(page_id) {}

  static HeapIteratorImpl HeapBegin(const bench::Heap* heap,
                                    const SlabMap* slab_map) {
    return HeapIteratorImpl(slab_map, PageId::FromPtr(heap->Start()));
  }
  static HeapIteratorImpl HeapEnd(const bench::Heap* heap,
                                  const SlabMap* slab_map) {
    return HeapIteratorImpl(slab_map, PageId::FromPtr(heap->End()));
  }

  bool operator==(HeapIteratorImpl other) const {
    return current_ == other.current_;
  }
  bool operator!=(HeapIteratorImpl other) const {
    return !(*this == other);
  }

  MappedSlab* operator*() {
    MappedSlab* slab = slab_map_->FindSlab(current_);
    CK_ASSERT_NE(slab, nullptr);
    CK_ASSERT_NE(slab->Type(), SlabType::kUnmapped);
    return slab;
  }
  MappedSlab* operator->() {
    return **this;
  }

  HeapIteratorImpl operator++() {
    Slab* current = **this;
    current_ += current->ToMapped()->Pages();
    return *this;
  }
  HeapIteratorImpl operator++(int) {
    HeapIteratorImpl copy = *this;
    ++*this;
    return copy;
  }

 private:
  const SlabMap* const slab_map_;
  PageId current_;
};

using HeapIterator = HeapIteratorImpl<SlabMap>;

}  // namespace ckmalloc

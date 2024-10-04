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
  using value_type = MappedSlab*;
  using difference_type = ssize_t;

  HeapIteratorImpl(SlabMap* slab_map, PageId page_id)
      : slab_map_(slab_map),
        current_slab_(slab_map->FindSlab(page_id)),
        current_(page_id) {}

  HeapIteratorImpl()
      : slab_map_(nullptr), current_slab_(nullptr), current_(PageId::Nil()) {}

  void swap(HeapIteratorImpl& other) {
    std::swap(slab_map_, other.slab_map_);
    std::swap(current_, other.current_);
  }

  static HeapIteratorImpl HeapBegin(const bench::Heap* heap,
                                    SlabMap* slab_map) {
    return HeapIteratorImpl(slab_map, PageId::FromPtr(heap->Start()));
  }

  bool operator==(const HeapIteratorImpl& other) const {
    return current_ == other.current_;
  }
  bool operator!=(const HeapIteratorImpl& other) const {
    return !(*this == other);
  }

  MappedSlab* operator*() const {
    CK_ASSERT_NE(current_slab_, nullptr);
    CK_ASSERT_NE(current_slab_->Type(), SlabType::kUnmapped);
    return current_slab_;
  }
  MappedSlab* operator->() const {
    return **this;
  }

  HeapIteratorImpl& operator++() {
    Slab* current = **this;
    current_ += current->ToMapped()->Pages();
    current_slab_ = slab_map_->FindSlab(current_);
    if (current_slab_ == nullptr) {
      current_ = PageId::Nil();
    }
    return *this;
  }
  HeapIteratorImpl operator++(int) {
    HeapIteratorImpl copy = *this;
    ++*this;
    return copy;
  }

 private:
  SlabMap* slab_map_;
  MappedSlab* current_slab_;
  PageId current_;
};

using HeapIterator = HeapIteratorImpl<SlabMap>;

}  // namespace ckmalloc

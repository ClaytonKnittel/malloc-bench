#pragma once

#include <cstdint>
#include <limits>

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

constexpr uint32_t kMaxPageIdx = 1 << (kHeapSizeShift - kPageShift);

class PageId {
  template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap>
  friend class SlabManagerImpl;

  template <MetadataAllocInterface MetadataAlloc>
  friend class SlabMapImpl;

  template <typename Sink>
  friend void AbslStringify(Sink&, const PageId&);

 public:
  constexpr explicit PageId(uint32_t page_idx) : page_idx_(page_idx) {
    CK_ASSERT_LT(page_idx, kMaxPageIdx);
  }

  PageId(const PageId& page_id) = default;
  PageId& operator=(const PageId& page_id) = default;

  bool operator==(const PageId& page_id) const {
    return page_idx_ == page_id.page_idx_;
  }
  bool operator!=(const PageId& page_id) const {
    return !(*this == page_id);
  }

  bool operator<(const PageId& page_id) const {
    return page_idx_ < page_id.page_idx_;
  }
  bool operator<=(const PageId& page_id) const {
    return !(page_id < *this);
  }
  bool operator>(const PageId& page_id) const {
    return page_id < *this;
  }
  bool operator>=(const PageId& page_id) const {
    return !(*this < page_id);
  }

  PageId operator+(uint32_t offset) const {
    return PageId(page_idx_ + offset);
  }
  PageId operator+=(uint32_t offset) {
    *this = (*this + offset);
    return *this;
  }

  PageId& operator++() {
    *this = *this + 1;
    return *this;
  }
  PageId operator++(int) {
    PageId copy = *this;
    ++*this;
    return copy;
  }

  PageId operator-(uint32_t offset) const {
    return PageId(page_idx_ - offset);
  }
  PageId operator-=(uint32_t offset) {
    *this = (*this - offset);
    return *this;
  }

  PageId& operator--() {
    *this = *this - 1;
    return *this;
  }
  PageId operator--(int) {
    PageId copy = *this;
    --*this;
    return copy;
  }

  int32_t operator-(const PageId& page_id) const {
    return static_cast<int32_t>(page_idx_) -
           static_cast<int32_t>(page_id.page_idx_);
  }

  // The id of the first page in the heap. This is reserved for the first
  // metadata slab.
  static constexpr PageId Zero() {
    return PageId(0);
  }

  static constexpr PageId Nil() {
    return PageId();
  }

 private:
  constexpr PageId() : page_idx_(std::numeric_limits<uint32_t>::max()) {}

  uint32_t Idx() const {
    return page_idx_;
  }

  // The index into the heap of this page, where idx 0 is the first page, idx 1
  // is the next page, and so on.
  uint32_t page_idx_;
};

}  // namespace ckmalloc

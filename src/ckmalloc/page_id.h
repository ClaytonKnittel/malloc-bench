#pragma once

#include <cinttypes>
#include <cstdint>
#include <limits>
#include <ostream>

#include "absl/strings/str_format.h"

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
  friend void AbslStringify(Sink&, PageId);

  friend inline std::ostream& operator<<(std::ostream& ostr, PageId page_id);

 public:
  // Constructs a `Nil` `PageId`.
  constexpr PageId() : page_idx_(std::numeric_limits<uint32_t>::max()) {}
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
  uint32_t Idx() const {
    return page_idx_;
  }

  // The index into the heap of this page, where idx 0 is the first page, idx 1
  // is the next page, and so on.
  uint32_t page_idx_;
};

template <typename Sink>
void AbslStringify(Sink& sink, PageId page_id) {
  absl::Format(&sink, "%" PRIu32, page_id.Idx());
}

inline std::ostream& operator<<(std::ostream& ostr, PageId page_id) {
  return ostr << page_id.Idx();
}

}  // namespace ckmalloc

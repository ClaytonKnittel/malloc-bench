#pragma once

#include <cinttypes>
#include <cstdint>
#include <limits>
#include <ostream>

#include "absl/strings/str_format.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

constexpr uint64_t kMaxPageIdx = UINT64_C(1) << (kAddressBits - kPageShift);

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
  constexpr PageId() : page_idx_(std::numeric_limits<uint64_t>::max()) {}
  constexpr explicit PageId(uint64_t page_idx) : page_idx_(page_idx) {
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

  PageId operator+(uint64_t offset) const {
    return PageId(page_idx_ + offset);
  }
  PageId operator+=(uint64_t offset) {
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

  PageId operator-(uint64_t offset) const {
    return PageId(page_idx_ - offset);
  }
  PageId operator-=(uint64_t offset) {
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

  int64_t operator-(const PageId& page_id) const {
    return static_cast<int64_t>(page_idx_) -
           static_cast<int64_t>(page_id.page_idx_);
  }

  static constexpr PageId Nil() {
    return PageId();
  }

  // Returns the `PageId` for the page containing `ptr`.
  static PageId FromPtr(const void* ptr) {
    CK_ASSERT_LT(reinterpret_cast<uint64_t>(ptr), UINT64_C(1) << kAddressBits);
    return PageId(reinterpret_cast<uint64_t>(ptr) / kPageSize);
  }

  // Returns a pointer to the start of the page for this `PageId`.
  void* PageStart() const {
    void* page_start =
        // NOLINTNEXTLINE(performance-no-int-to-ptr)
        reinterpret_cast<void*>(static_cast<uintptr_t>(Idx()) * kPageSize);
    CK_ASSERT_LT(reinterpret_cast<uint64_t>(page_start), UINT64_C(1)
                                                             << kAddressBits);
    return page_start;
  }

 private:
  uint64_t Idx() const {
    return page_idx_;
  }

  // The index into the heap of this page, where idx 0 is the first page, idx 1
  // is the next page, and so on.
  uint64_t page_idx_;
};

template <typename Sink>
void AbslStringify(Sink& sink, PageId page_id) {
  absl::Format(&sink, "%" PRIu32, page_id.Idx());
}

inline std::ostream& operator<<(std::ostream& ostr, PageId page_id) {
  return ostr << page_id.Idx();
}

}  // namespace ckmalloc

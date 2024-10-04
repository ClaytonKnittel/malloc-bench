#pragma once

#include <cstdint>
#include <limits>
#include <ostream>
#include <type_traits>

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/size_class.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

// Slice id's are offsets from the beginning of the slab of the slice in a
// small slab, in multiples of `kMinAlignment`.
template <typename T>
requires std::is_integral_v<T>
class SliceId {
  friend class SmallSlab;

  template <typename U>
  requires std::is_integral_v<U>
  friend std::ostream& operator<<(std::ostream&, const SliceId<U>&);

 public:
  static constexpr SliceId FromOffset(uint64_t offset_bytes,
                                      SizeClass size_class) {
    CK_ASSERT_LT(offset_bytes, kPageSize);
    return SliceId(size_class.OffsetToIdx(offset_bytes));
  }

  static constexpr SliceId FromIdx(T idx) {
    return SliceId(idx);
  }

  // Allow copy construction/assignment.
  constexpr SliceId(const SliceId&) = default;
  constexpr SliceId& operator=(const SliceId&) = default;

  static constexpr SliceId Nil() {
    return SliceId();
  }

  constexpr bool operator==(const SliceId& other) const {
    return Id() == other.Id();
  }
  constexpr bool operator!=(const SliceId& other) const {
    return !(*this == other);
  }

  constexpr T Id() const {
    if (id_ != kNilId) {
      CK_ASSERT_LT(id_, kPageSize / kMinAlignment);
    }
    return id_;
  }

  constexpr uint32_t SliceOffsetBytes(SizeClass size_class) const {
    return static_cast<uint32_t>(Id() * size_class.SliceSize());
  }

 private:
  static constexpr T kNilId = std::numeric_limits<T>::max();

  constexpr explicit SliceId(T id) : id_(id) {}

  constexpr SliceId() : id_(kNilId) {}

  // The index of the slice in the slab.
  T id_;
};

template <typename T>
requires std::is_integral_v<T>
std::ostream& operator<<(std::ostream& ostr, const SliceId<T>& slice_id) {
  if (slice_id == SliceId<T>::Nil()) {
    return ostr << "[nil]";
  }
  return ostr << static_cast<uint32_t>(slice_id.Id());
}

using SmallSliceId = SliceId<uint8_t>;
using TinySliceId = SliceId<uint16_t>;

}  // namespace ckmalloc

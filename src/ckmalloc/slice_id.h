#pragma once

#include <cstdint>
#include <limits>
#include <ostream>

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

// Slice id's are offsets from the beginning of the slab of the slice in a
// small slab, in multiples of `kMinAlignment`.
//
// TODO: If these are indexes, we can use 8 bytes only for 16+ small slabs.
class SliceId {
  friend class SmallSlab;

  friend inline std::ostream& operator<<(std::ostream&, const SliceId&);

 public:
  constexpr explicit SliceId(uint64_t offset_bytes)
      : id_(offset_bytes / kMinAlignment) {
    CK_ASSERT_LT(offset_bytes, kPageSize);
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

  constexpr uint16_t Id() const {
    if (id_ != kNilId) {
      CK_ASSERT_LT(id_, kPageSize / kMinAlignment);
    }
    return id_;
  }

  constexpr uint32_t SliceOffsetBytes() const {
    return Id() * kMinAlignment;
  }

 private:
  static constexpr uint16_t kNilId = std::numeric_limits<uint16_t>::max();

  constexpr SliceId() : id_(kNilId) {}

  // The index of the slice in the slab.
  uint16_t id_;
};

inline std::ostream& operator<<(std::ostream& ostr, const SliceId& slice_id) {
  return ostr << slice_id.id_;
}

}  // namespace ckmalloc

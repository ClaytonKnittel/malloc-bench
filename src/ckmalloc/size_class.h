#pragma once

#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ostream>

#include "absl/strings/str_format.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

std::ostream& operator<<(std::ostream& ostr, ckmalloc::SizeClass size_class);

// A size class is an allowed size of slices in a small slab, which holds an
// array of equally-sized slices of memory for individual allocation.
class SizeClass {
 public:
  static constexpr size_t kNumSizeClasses = 21;

  static constexpr size_t kNumSizeClassLookupIdx =
      kMaxSmallSize / kDefaultAlignment + 1;

  struct SizeClassInfo {
    // The maximum allocation size that fits in allocations for this size class.
    uint16_t max_size;

    // The number of allocations that fit in a single slab holding allocations
    // of this size class.
    uint16_t slices_per_slab;
  };
  static const std::array<SizeClassInfo, SizeClass::kNumSizeClasses>
      kSizeClassInfo;

  constexpr SizeClass() = default;

  static constexpr SizeClass Nil() {
    return SizeClass();
  }

  static SizeClass FromOrdinal(size_t ord) {
    CK_ASSERT_LT(ord, kNumSizeClasses);
    return SizeClass(ord);
  }

  static SizeClass FromUserDataSize(size_t user_size) {
    CK_ASSERT_LE(user_size, kMaxSmallSize);
    CK_ASSERT_NE(user_size, 0);
    size_t idx = OrdinalMapIdx(user_size);
    return SizeClass(kOrdinalMap[idx]);
  }

  static SizeClass FromSliceSize(uint64_t slice_size) {
    CK_ASSERT_LE(slice_size, kMaxSmallSize);
    CK_ASSERT_NE(slice_size, 0);
    CK_ASSERT_TRUE(slice_size == kMinAlignment ||
                   slice_size % kDefaultAlignment == 0);

    return FromUserDataSize(slice_size);
  }

  bool operator==(SizeClass other) const {
    return ordinal_ == other.ordinal_;
  }
  bool operator!=(SizeClass other) const {
    return !(*this == other);
  }

  // Returns the size of slices represented by this size class.
  uint64_t SliceSize() const {
    CK_ASSERT_NE(*this, Nil());
    return kSizeClassInfo[Ordinal()].max_size;
  }

  // Returns a number 0 - `kNumSizeClasses`-1,
  size_t Ordinal() const {
    CK_ASSERT_NE(*this, Nil());
    return ordinal_;
  }

  // The number of slices that can fit into a small slab of this size class.
  uint32_t MaxSlicesPerSlab() const {
    CK_ASSERT_NE(*this, Nil());
    return kSizeClassInfo[Ordinal()].slices_per_slab;
  }

  // TODO check if this is the fastest way to do this.
  uint32_t OffsetToIdx(uint64_t offset_bytes) const {
    static_assert(kNumSizeClasses == 21);
    switch (Ordinal()) {
      // NOLINTNEXTLINE(bugprone-branch-clone)
      case 0:
        return static_cast<uint32_t>(offset_bytes / SliceSize());
      case 1:
        return static_cast<uint32_t>(offset_bytes / SliceSize());
      case 2:
        return static_cast<uint32_t>(offset_bytes / SliceSize());
      case 3:
        return static_cast<uint32_t>(offset_bytes / SliceSize());
      case 4:
        return static_cast<uint32_t>(offset_bytes / SliceSize());
      case 5:
        return static_cast<uint32_t>(offset_bytes / SliceSize());
      case 6:
        return static_cast<uint32_t>(offset_bytes / SliceSize());
      case 7:
        return static_cast<uint32_t>(offset_bytes / SliceSize());
      case 8:
        return static_cast<uint32_t>(offset_bytes / SliceSize());
      case 9:
        return static_cast<uint32_t>(offset_bytes / SliceSize());
      case 10:
        return static_cast<uint32_t>(offset_bytes / SliceSize());
      case 11:
        return static_cast<uint32_t>(offset_bytes / SliceSize());
      case 12:
        return static_cast<uint32_t>(offset_bytes / SliceSize());
      case 13:
        return static_cast<uint32_t>(offset_bytes / SliceSize());
      case 14:
        return static_cast<uint32_t>(offset_bytes / SliceSize());
      case 15:
        return static_cast<uint32_t>(offset_bytes / SliceSize());
      case 16:
        return static_cast<uint32_t>(offset_bytes / SliceSize());
      case 17:
        return static_cast<uint32_t>(offset_bytes / SliceSize());
      case 18:
        return static_cast<uint32_t>(offset_bytes / SliceSize());
      case 19:
        return static_cast<uint32_t>(offset_bytes / SliceSize());
      case 20:
        return static_cast<uint32_t>(offset_bytes / SliceSize());
      case 21:
        return static_cast<uint32_t>(offset_bytes / SliceSize());
      default:
        CK_UNREACHABLE();
    }
  }

  // Given a user size, returns the index into the ordinal map. This is called
  // on every allocation.
  //
  // TODO: See if it's better to do CeilDiv(user_size, 8) and have ordinal map
  // be 2x larger.
  static constexpr size_t OrdinalMapIdx(size_t user_size) {
    if (user_size <= 8) {
      return 0;
    }
    return CeilDiv(user_size, kDefaultAlignment);
  }

 private:
  static constexpr uint8_t kNilOrdinal = std::numeric_limits<uint8_t>::max();

  // A map from a quickly-computed index (from `OrdinalMapIdx()`) derived from a
  // user allocation request size to the corresponding size class that holds
  // allocations of that size.
  static const std::array<SizeClass, SizeClass::kNumSizeClassLookupIdx>
      kOrdinalMap;

  explicit SizeClass(uint8_t ord) : ordinal_(ord) {}

  uint8_t ordinal_ = kNilOrdinal;
};

static_assert(SizeClass::OrdinalMapIdx(kMaxSmallSize) + 1 ==
              SizeClass::kNumSizeClassLookupIdx);

template <typename Sink>
void AbslStringify(Sink& sink, ckmalloc::SizeClass size_class) {
  absl::Format(&sink, "[%" PRIu64 "]", size_class.SliceSize());
}

}  // namespace ckmalloc

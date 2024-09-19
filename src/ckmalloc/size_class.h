#pragma once

#include <cinttypes>
#include <cstdint>
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
  static constexpr size_t kNumSizeClasses = 14;

  static constexpr size_t kNumSizeClassLookupIdx =
      kMaxSmallSize / kDefaultAlignment + 1;

  constexpr SizeClass() : ordinal_(0) {}

  static constexpr SizeClass Nil() {
    return SizeClass();
  }

  static constexpr SizeClass FromOrdinal(size_t ord) {
    CK_ASSERT_LT(ord, kNumSizeClasses);
    return SizeClass(ord + 1);
  }

  static SizeClass FromUserDataSize(size_t user_size) {
    CK_ASSERT_LE(user_size, kMaxSmallSize);
    CK_ASSERT_NE(user_size, 0);
    return FromSliceSize(user_size <= kMinAlignment
                             ? kMinAlignment
                             : AlignUp(user_size, kDefaultAlignment));
  }

  static SizeClass FromSliceSize(uint64_t slice_size);

  constexpr bool operator==(SizeClass other) const {
    return ordinal_ == other.ordinal_;
  }
  constexpr bool operator!=(SizeClass other) const {
    return !(*this == other);
  }

  // Returns the size of slices represented by this size class.
  uint64_t SliceSize() const;

  // Returns a number 0 - `kNumSizeClasses`-1,
  constexpr size_t Ordinal() const {
    CK_ASSERT_NE(*this, Nil());
    return ordinal_ - 1;
  }

  // The number of slices that can fit into a small slab of this size class.
  uint32_t MaxSlicesPerSlab() const;

  // TODO check if this is the fastest way to do this.
  constexpr uint32_t OffsetToIdx(uint64_t offset_bytes) const {
    static_assert(kNumSizeClasses == 14);
    switch (Ordinal()) {
      case 0:
        return offset_bytes / 8;
      case 1:
        return offset_bytes / 16;
      case 2:
        return offset_bytes / 32;
      case 3:
        return offset_bytes / 48;
      case 4:
        return offset_bytes / 64;
      case 5:
        return offset_bytes / 80;
      case 6:
        return offset_bytes / 96;
      case 7:
        return offset_bytes / 112;
      case 8:
        return offset_bytes / 128;
      case 9:
        return offset_bytes / 144;
      case 10:
        return offset_bytes / 160;
      case 12:
        return offset_bytes / 192;
      case 14:
        return offset_bytes / 224;
      case 16:
        return offset_bytes / 256;
      default:
        __builtin_unreachable();
    }
  }

 private:
  explicit constexpr SizeClass(uint8_t ord) : ordinal_(ord) {}

  uint8_t ordinal_;
};

template <typename Sink>
void AbslStringify(Sink& sink, ckmalloc::SizeClass size_class) {
  absl::Format(&sink, "[%" PRIu64 "]", size_class.SliceSize());
}

}  // namespace ckmalloc

#pragma once

#include <array>
#include <cinttypes>
#include <cstdint>

#include "absl/strings/str_format.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

// A size class is an allowed size of slices in a small slab, which holds an
// array of equally-sized slices of memory for individual allocation.
class SizeClass {
 public:
  // The number of size classes is the count of 16-byte multiples up to
  // `kMaxSmallSize`, plus 1 for 8-byte slices.
  static constexpr size_t kNumSizeClasses =
      kMaxSmallSize / kDefaultAlignment + 1;

  static constexpr SizeClass FromOrdinal(size_t ord) {
    CK_ASSERT_LT(ord, kNumSizeClasses);
    return FromSliceSize(
        std::max<uint64_t>(ord * kDefaultAlignment, kMinAlignment));
  }

  static constexpr SizeClass FromUserDataSize(size_t user_size) {
    CK_ASSERT_LE(user_size, kMaxSmallSize);
    CK_ASSERT_NE(user_size, 0);
    return FromSliceSize(user_size <= kMinAlignment
                             ? kMinAlignment
                             : AlignUp(user_size, kDefaultAlignment));
  }

  static constexpr SizeClass FromSliceSize(uint64_t slice_size) {
    CK_ASSERT_LE(slice_size, kMaxSmallSize);
    CK_ASSERT_NE(slice_size, 0);
    CK_ASSERT_TRUE(slice_size == kMinAlignment ||
                   slice_size % kDefaultAlignment == 0);

    return SizeClass(static_cast<uint32_t>(slice_size));
  }

  constexpr bool operator==(SizeClass other) const {
    return size_class_ == other.size_class_;
  }
  constexpr bool operator!=(SizeClass other) const {
    return !(*this == other);
  }

  // Returns the size of slices represented by this size class.
  constexpr uint64_t SliceSize() const {
    return static_cast<uint64_t>(size_class_ * kSizeClassDivisor);
  }

  // Returns a number 0 - `kNumSizeClasses`-1,
  constexpr size_t Ordinal() const {
    return size_class_ / (kDefaultAlignment / kSizeClassDivisor);
  }

  // The number of slices that can fit into a small slab of this size class.
  constexpr uint32_t MaxSlicesPerSlab() const {
    return GenerateSliceCountMap()[Ordinal()];
  }

  // TODO check if this is the fastest way to do this.
  constexpr uint32_t OffsetToIdx(uint64_t offset_bytes) const {
    static_assert(kNumSizeClasses == 17);
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
      case 11:
        return offset_bytes / 176;
      case 12:
        return offset_bytes / 192;
      case 13:
        return offset_bytes / 208;
      case 14:
        return offset_bytes / 224;
      case 15:
        return offset_bytes / 240;
      case 16:
        return offset_bytes / 256;
      default:
        __builtin_unreachable();
    }
  }

 private:
  explicit constexpr SizeClass(uint32_t size_class)
      : size_class_(size_class / kSizeClassDivisor) {}

  static constexpr std::array<uint32_t, kNumSizeClasses>
  GenerateSliceCountMap() {
    std::array<uint32_t, kNumSizeClasses> slice_map;
    for (uint32_t ord = 0; ord < kNumSizeClasses; ord++) {
      slice_map[ord] = kPageSize / SizeClass::FromOrdinal(ord).SliceSize();
    }
    return slice_map;
  }

  static constexpr uint64_t kSizeClassDivisor = kMinAlignment;

  // The size in bytes of slices / `kSizeClassDivisor` for this size class.
  uint8_t size_class_;
};

template <typename Sink>
void AbslStringify(Sink& sink, SizeClass size_class) {
  absl::Format(&sink, "[%" PRIu64 "]", size_class.SliceSize());
}

}  // namespace ckmalloc

#pragma once

#include <cstdint>

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

// A size class is an allowed size of slices in a small slab, which holds an
// array of equally-sized slices of memory for individual allocation.
class SizeClass {
 public:
  static constexpr SizeClass FromSliceSize(uint64_t slice_size) {
    CK_ASSERT_LE(slice_size, kMaxSmallSize);
    CK_ASSERT_NE(slice_size, 0);
    CK_ASSERT_TRUE(slice_size == kMinAlignment ||
                   slice_size % kDefaultAlignment == 0);

    return SizeClass(static_cast<uint8_t>(slice_size));
  }

  // Returns the size of slices represented by this size class.
  constexpr uint64_t SliceSize() const {
    return static_cast<uint64_t>(size_class_);
  }

  constexpr uint32_t MaxSlicesPerSlab() const {
    static_assert(kMaxSmallSize == 128);
    constexpr uint32_t kSliceMap[9] = {
      kPageSize / 8,  kPageSize / 16,  kPageSize / 32,
      kPageSize / 48, kPageSize / 64,  kPageSize / 80,
      kPageSize / 96, kPageSize / 112, kPageSize / 128,
    };

    return kSliceMap[size_class_ / kDefaultAlignment];
  }

  // TODO check if this is the fastest way to do this.
  constexpr uint32_t OffsetToIdx(uint64_t offset_bytes) const {
    switch (size_class_ / kDefaultAlignment) {
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
      default:
        __builtin_unreachable();
    }
  }

 private:
  explicit constexpr SizeClass(uint8_t size_class) : size_class_(size_class) {}

  // The size in bytes of slices for this size class.
  uint8_t size_class_;
};

}  // namespace ckmalloc

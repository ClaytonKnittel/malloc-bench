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

    return SizeClass(static_cast<uint8_t>(slice_size / kDefaultAlignment));
  }

  // Returns the size of slices represented by this size class.
  constexpr uint64_t SliceSize() const {
    return std::max<uint64_t>(kDefaultAlignment * size_class_, kMinAlignment);
  }

  constexpr uint32_t MaxSlicesPerSlab() const {
    static_assert(kMaxSmallSize == 128);
    CK_ASSERT_LT(size_class_, 9);
    constexpr uint32_t kSliceMap[9] = {
      kPageSize / 8,  kPageSize / 16,  kPageSize / 32,
      kPageSize / 48, kPageSize / 64,  kPageSize / 80,
      kPageSize / 96, kPageSize / 112, kPageSize / 128,
    };

    return kSliceMap[size_class_];
  }

 private:
  explicit constexpr SizeClass(uint8_t size_class) : size_class_(size_class) {}

  // 1/16 of the actual size of slices, unless 0 in which case slices are 8
  // bytes.
  uint8_t size_class_;
};

}  // namespace ckmalloc
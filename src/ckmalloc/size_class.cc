#include "src/ckmalloc/size_class.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ostream>

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

namespace {

struct SizeClassParams {
  uint16_t max_size;
};

static_assert(SizeClass::kNumSizeClasses == 14);
constexpr SizeClassParams kSizeClassParams[SizeClass::kNumSizeClasses] = {
  // clang-format off
      { /*max_size=*/8 },
      { /*max_size=*/16 },
      { /*max_size=*/32 },
      { /*max_size=*/48 },
      { /*max_size=*/64 },
      { /*max_size=*/80 },
      { /*max_size=*/96 },
      { /*max_size=*/112 },
      { /*max_size=*/128 },
      { /*max_size=*/144 },
      { /*max_size=*/160 },
      { /*max_size=*/192 },
      { /*max_size=*/224 },
      { /*max_size=*/256 },
  // clang-format on
};

// This is derived from `kSizeClassParams`.
struct SizeClassInfo {
  uint16_t max_size;
  uint16_t slices_per_slab;
};

constexpr std::array<SizeClassInfo, SizeClass::kNumSizeClasses> kSizeClassInfo =
    []() constexpr {
      std::array<SizeClassInfo, SizeClass::kNumSizeClasses> size_class_info;
      for (size_t ord = 0; ord < SizeClass::kNumSizeClasses; ord++) {
        uint16_t max_size = kSizeClassParams[ord].max_size;
        size_class_info[ord] = {
          .max_size = max_size,
          .slices_per_slab = static_cast<uint16_t>(kPageSize / max_size),
        };
      }
      return size_class_info;
    }();

constexpr size_t OrdinalMapIdx(size_t user_size) {
  if (user_size <= 8) {
    return 0;
  }
  return CeilDiv(user_size, kDefaultAlignment);
}
static_assert(OrdinalMapIdx(kMaxSmallSize) + 1 ==
              SizeClass::kNumSizeClassLookupIdx);

// TODO: Check if cheaper to do this lookup or to recompute on the fly.
constexpr std::array<uint8_t, SizeClass::kNumSizeClassLookupIdx> kOrdinalMap =
    []() -> std::array<uint8_t, SizeClass::kNumSizeClassLookupIdx> {
  std::array<uint8_t, SizeClass::kNumSizeClassLookupIdx> ordinal_map;

  size_t info_idx = 0;
  size_t map_idx = 0;
  while (info_idx < SizeClass::kNumSizeClasses) {
    uint64_t size = kSizeClassInfo[info_idx].max_size;
    size_t next_map_idx = OrdinalMapIdx(size);
    while (map_idx <= next_map_idx) {
      assert(map_idx <= std::numeric_limits<uint8_t>::max());
      ordinal_map[map_idx] = info_idx;
      map_idx++;
    }

    info_idx++;
  }
  assert(info_idx == SizeClass::kNumSizeClasses);

  return ordinal_map;
}();

}  // namespace

/* static */
SizeClass SizeClass::FromUserDataSize(size_t user_size) {
  CK_ASSERT_LE(user_size, kMaxSmallSize);
  CK_ASSERT_NE(user_size, 0);
  size_t idx = OrdinalMapIdx(user_size);
  return SizeClass(kOrdinalMap[idx]);
}

/* static */
SizeClass SizeClass::FromSliceSize(uint64_t slice_size) {
  CK_ASSERT_LE(slice_size, kMaxSmallSize);
  CK_ASSERT_NE(slice_size, 0);
  CK_ASSERT_TRUE(slice_size == kMinAlignment ||
                 slice_size % kDefaultAlignment == 0);

  return FromUserDataSize(slice_size);
}

uint64_t SizeClass::SliceSize() const {
  CK_ASSERT_NE(*this, Nil());
  return kSizeClassInfo[Ordinal()].max_size;
}

uint32_t SizeClass::MaxSlicesPerSlab() const {
  CK_ASSERT_NE(*this, Nil());
  return kSizeClassInfo[Ordinal()].slices_per_slab;
}

std::ostream& operator<<(std::ostream& ostr, ckmalloc::SizeClass size_class) {
  if (size_class == SizeClass::Nil()) {
    return ostr << "[0]";
  } else {
    return ostr << "[" << size_class.SliceSize() << "]";
  }
}

}  // namespace ckmalloc

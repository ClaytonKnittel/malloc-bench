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

static_assert(SizeClass::kNumSizeClasses == 21);
static_assert(kMaxSmallSize == 512);
const SizeClassParams kSizeClassParams[SizeClass::kNumSizeClasses] = {
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
      { /*max_size=*/272 },
      { /*max_size=*/304 },
      { /*max_size=*/336 },
      { /*max_size=*/368 },
      { /*max_size=*/400 },
      { /*max_size=*/448 },
      { /*max_size=*/512 },
  // clang-format on
};

}  // namespace

const std::array<SizeClass::SizeClassInfo, SizeClass::kNumSizeClasses>
    SizeClass::kSizeClassInfo = []() {
      std::array<SizeClass::SizeClassInfo, SizeClass::kNumSizeClasses>
          size_class_info;
      for (size_t ord = 0; ord < SizeClass::kNumSizeClasses; ord++) {
        uint16_t max_size = kSizeClassParams[ord].max_size;
        size_class_info[ord] = {
          .max_size = max_size,
          .slices_per_slab = static_cast<uint16_t>(kPageSize / max_size),
        };
      }
      return size_class_info;
    }();

// TODO: Check if cheaper to do this lookup or to recompute on the fly.
const std::array<SizeClass, SizeClass::kNumSizeClassLookupIdx>
    SizeClass::kOrdinalMap =
        []() -> std::array<SizeClass, SizeClass::kNumSizeClassLookupIdx> {
  std::array<SizeClass, SizeClass::kNumSizeClassLookupIdx> ordinal_map;

  size_t info_idx = 0;
  size_t map_idx = 0;
  while (info_idx < SizeClass::kNumSizeClasses) {
    uint64_t size = SizeClass::kSizeClassInfo[info_idx].max_size;
    size_t next_map_idx = SizeClass::OrdinalMapIdx(size);
    CK_ASSERT_GE(next_map_idx, map_idx);
    while (map_idx <= next_map_idx) {
      CK_ASSERT_LE(map_idx, std::numeric_limits<uint8_t>::max());
      ordinal_map[map_idx] = SizeClass::FromOrdinal(info_idx);
      map_idx++;
    }

    info_idx++;
  }
  assert(info_idx == SizeClass::kNumSizeClasses);

  return ordinal_map;
}();

std::ostream& operator<<(std::ostream& ostr, ckmalloc::SizeClass size_class) {
  if (size_class == SizeClass::Nil()) {
    return ostr << "[0]";
  } else {
    return ostr << "[" << size_class.SliceSize() << "]";
  }
}

}  // namespace ckmalloc

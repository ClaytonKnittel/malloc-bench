#pragma once

#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
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
  static constexpr size_t kNumSizeClasses = 26;

  static constexpr size_t kNumSizeClassLookupIdx =
      kMaxSmallSize / kDefaultAlignment + 1;

  struct SizeClassInfo {
    // The maximum allocation size that fits in allocations for this size class.
    uint16_t max_size;

    // The number of pages that slabs of this size class span.
    uint8_t pages;

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

  static constexpr SizeClass FromOrdinal(size_t ord) {
    CK_ASSERT_LT(ord, kNumSizeClasses);
    return SizeClass(ord);
  }

  static SizeClass FromUserDataSize(
      size_t user_size, std::optional<size_t> alignment = std::nullopt);

  static SizeClass FromSliceSize(
      uint64_t slice_size, std::optional<size_t> alignment = std::nullopt);

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
    return ordinal_;
  }

  // Returns the number of pages a small slab of this size class should span.
  uint32_t Pages() const;

  // The number of slices that can fit into a small slab of this size class.
  uint32_t MaxSlicesPerSlab() const;

  // Given an offset in a small slab of `this` size class in bytes, returns the
  // index of the slice.
  uint32_t OffsetToIdx(uint64_t offset_bytes) const;

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

  explicit constexpr SizeClass(uint8_t ord) : ordinal_(ord) {}

  uint8_t ordinal_ = kNilOrdinal;
};

static_assert(SizeClass::OrdinalMapIdx(kMaxSmallSize) + 1 ==
              SizeClass::kNumSizeClassLookupIdx);

template <typename Sink>
void AbslStringify(Sink& sink, ckmalloc::SizeClass size_class) {
  absl::Format(&sink, "[%" PRIu64 "]", size_class.SliceSize());
}

}  // namespace ckmalloc

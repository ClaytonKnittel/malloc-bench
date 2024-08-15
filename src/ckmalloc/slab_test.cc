#include "src/ckmalloc/slab.h"

#include <array>
#include <cinttypes>
#include <cstdint>
#include <tuple>
#include <type_traits>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "gtest/gtest.h"
#include "util/absl_util.h"
#include "util/gtest_util.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/size_class.h"
#include "src/ckmalloc/slice.h"
#include "src/ckmalloc/slice_id.h"
#include "src/ckmalloc/util.h"
#include "src/rng.h"

namespace ckmalloc {

using util::IsOk;

template <typename T>
requires std::is_integral_v<T>
class TestSmallSlab {
 public:
  explicit TestSmallSlab(SizeClass size_class) {
    static_cast<Slab*>(&slab_)->Init<SmallSlab>(
        PageId::Zero(), /*n_pages=*/UINT32_C(1), size_class);
  }

  SmallSlab& Underlying() {
    return slab_;
  }

  SmallSlabMetadata<T>& Metadata() {
    if constexpr (std::is_same_v<T, uint16_t>) {
      return slab_.TinyMetadata();
    } else {
      return slab_.SmallMetadata();
    }
  }

  SizeClass SizeClass() {
    return Metadata().SizeClass();
  }

  Slice* SliceAt(SliceId<T> slice_id) {
    CK_ASSERT_NE(slice_id, SliceId<T>::Nil());
    return reinterpret_cast<Slice*>(
        &data_[slice_id.SliceOffsetBytes(SizeClass()) / sizeof(uint64_t)]);
  }

  SliceId<T> IdForSlice(Slice* slice) {
    return SliceId<T>::FromOffset(PtrDistance(slice, data_.data()),
                                  SizeClass());
  }

  absl::StatusOr<AllocatedSlice*> AllocSlice() {
    const class SizeClass size_class = Metadata().SizeClass();
    CK_ASSERT_LT(allocated_slices_.size(), size_class.MaxSlicesPerSlab());

    AllocatedSlice* slice = Metadata().PopSlice(data_.data());

    if (reinterpret_cast<uint64_t*>(slice) < data_.data() ||
        reinterpret_cast<uint64_t*>(slice) >=
            &data_[kPageSize / sizeof(uint64_t)]) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Allocated slice outside the range of the slab: allocated %p, slab "
          "ranges from %p to %p",
          slice, data_.data(), &data_[kPageSize / sizeof(uint64_t)]));
    }

    if (PtrDistance(slice, data_.data()) % size_class.SliceSize() != 0) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Allocated unaligned slice: offset %zu from the beginning of the "
          "slice, but size class is %" PRIu64,
          PtrDistance(slice, data_.data()), size_class.SliceSize()));
    }

    auto [it, inserted] = allocated_slices_.insert(slice);
    if (!inserted) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Allocated duplicate slice %p (id %" PRIu16 ")",
                          slice, IdForSlice(slice).Id()));
    }

    if (allocated_slices_.size() == size_class.MaxSlicesPerSlab()) {
      if (!Metadata().Full()) {
        return absl::FailedPreconditionError(
            absl::StrFormat("Expected slab to be full with %" PRIu64
                            " allocations, but Full() returned false",
                            allocated_slices_.size()));
      }
    } else if (Metadata().Full()) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Expected slab to be non-full with %" PRIu64
                          " allocations, but Full() returned true",
                          allocated_slices_.size()));
    }

    if (Metadata().Empty()) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Expected slab to be non-empty with %" PRIu64
                          " allocations, but Empty() returned true",
                          allocated_slices_.size()));
    }

    FillMagic(slice);
    return slice;
  }

  absl::Status FreeSlice(AllocatedSlice* slice) {
    auto it = allocated_slices_.find(slice);
    CK_ASSERT_TRUE(it != allocated_slices_.end());
    allocated_slices_.erase(it);

    RETURN_IF_ERROR(CheckMagic(slice));
    Metadata().PushSlice(data_.data(), slice->ToFree<T>());

    if (allocated_slices_.empty()) {
      if (!Metadata().Empty()) {
        return absl::FailedPreconditionError(
            absl::StrFormat("Expected slab to be empty with %" PRIu64
                            " allocations, but Empty() returned false",
                            allocated_slices_.size()));
      }
    } else if (Metadata().Empty()) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Expected slab to be non-empty with %" PRIu64
                          " allocations, but Empty() returned true",
                          allocated_slices_.size()));
    }

    if (Metadata().Full()) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Expected slab to be non-full with %" PRIu64
                          " allocations, but Full() returned true",
                          allocated_slices_.size()));
    }

    return absl::OkStatus();
  }

 private:
  void FillMagic(AllocatedSlice* slice) {
    uint64_t* begin = reinterpret_cast<uint64_t*>(slice);
    uint64_t* magic_begin =
        &magic_[PtrDistance(begin, data_.data()) / sizeof(uint64_t)];

    for (size_t i = 0;
         i < Metadata().SizeClass().SliceSize() / sizeof(uint64_t); i++) {
      uint64_t val = rng_.GenRand64();
      begin[i] = val;
      magic_begin[i] = val;
    }
  }

  absl::Status CheckMagic(AllocatedSlice* slice) {
    uint64_t* begin = reinterpret_cast<uint64_t*>(slice);
    uint64_t* magic_begin =
        &magic_[PtrDistance(begin, data_.data()) / sizeof(uint64_t)];

    for (size_t i = 0;
         i < Metadata().SizeClass().SliceSize() / sizeof(uint64_t); i++) {
      if (begin[i] != magic_begin[i]) {
        return absl::FailedPreconditionError(absl::StrFormat(
            "Dirtied allocated slice %" PRIu16 " at offset %zu: %016x vs %016x",
            IdForSlice(slice).Id(), i, begin[i], magic_begin[i]));
      }
    }

    return absl::OkStatus();
  }

  static util::Rng rng_;

  SmallSlab slab_;
  std::array<uint64_t, kPageSize / sizeof(uint64_t)> data_;
  std::array<uint64_t, kPageSize / sizeof(uint64_t)> magic_;

  absl::flat_hash_set<AllocatedSlice*> allocated_slices_;
};

using X = std::tuple<uint32_t, int>;
using Y = std::tuple_element_t<1, X>;

template <typename T>
requires std::is_integral_v<T>
util::Rng TestSmallSlab<T>::rng_ = util::Rng(1031, 5);

template <uint32_t S>
class SmallSlabTestParams {
 public:
  using IdType = std::conditional_t<S == 8 || S == 16, uint16_t, uint8_t>;
  static constexpr uint32_t kSizeClass = S;
};

template <typename P>
class SmallSlabTest : public ::testing::TestWithParam<uint64_t> {
 public:
  using T = P::IdType;

  static SizeClass GetSizeClass() {
    return SizeClass::FromSliceSize(P::kSizeClass);
  }

  static TestSmallSlab<T> MakeSlab() {
    return TestSmallSlab<T>(GetSizeClass());
  }

 private:
  SmallSlab small_slab_;
};

using Types =
    ::testing::Types<SmallSlabTestParams<8>, SmallSlabTestParams<16>,
                     SmallSlabTestParams<32>, SmallSlabTestParams<48>,
                     SmallSlabTestParams<64>, SmallSlabTestParams<80>,
                     SmallSlabTestParams<96>, SmallSlabTestParams<112>,
                     SmallSlabTestParams<128> >;
TYPED_TEST_SUITE(SmallSlabTest, Types);

TYPED_TEST(SmallSlabTest, EmptySmallSlab) {
  auto slab = SmallSlabTest<TypeParam>::MakeSlab();
}

TYPED_TEST(SmallSlabTest, SingleAllocation) {
  auto slab = SmallSlabTest<TypeParam>::MakeSlab();
  ASSERT_OK_AND_DEFINE(AllocatedSlice*, slice, slab.AllocSlice());
  EXPECT_EQ(slice,
            slab.SliceAt(SliceId<typename TypeParam::IdType>::FromIdx(0)));
}

TYPED_TEST(SmallSlabTest, SingleFree) {
  auto slab = SmallSlabTest<TypeParam>::MakeSlab();
  ASSERT_OK_AND_DEFINE(AllocatedSlice*, slice, slab.AllocSlice());
  EXPECT_THAT(slab.FreeSlice(slice), IsOk());
}

TYPED_TEST(SmallSlabTest, AllAllocations) {
  auto slab = SmallSlabTest<TypeParam>::MakeSlab();
  const uint64_t slices_per_slab =
      slab.Metadata().SizeClass().MaxSlicesPerSlab();

  for (size_t i = 0; i < slices_per_slab; i++) {
    ASSERT_OK_AND_DEFINE(AllocatedSlice*, slice, slab.AllocSlice());
    ASSERT_EQ(slice,
              slab.SliceAt(SliceId<typename TypeParam::IdType>::FromIdx(i)));
  }
}

TYPED_TEST(SmallSlabTest, FillUpThenEmpty) {
  auto slab = SmallSlabTest<TypeParam>::MakeSlab();
  const uint64_t slices_per_slab =
      slab.Metadata().SizeClass().MaxSlicesPerSlab();

  for (size_t i = 0; i < slices_per_slab; i++) {
    ASSERT_THAT(slab.AllocSlice().status(), IsOk());
  }

  for (size_t i = 0; i < slices_per_slab; i++) {
    ASSERT_THAT(slab.FreeSlice(static_cast<AllocatedSlice*>(slab.SliceAt(
                    SliceId<typename TypeParam::IdType>::FromIdx(i)))),
                IsOk());
  }
}

TYPED_TEST(SmallSlabTest, FillUpThenEmptyStrangeOrder) {
  auto slab = SmallSlabTest<TypeParam>::MakeSlab();
  const uint64_t slices_per_slab =
      slab.Metadata().SizeClass().MaxSlicesPerSlab();

  for (size_t i = 0; i < slices_per_slab; i++) {
    ASSERT_THAT(slab.AllocSlice().status(), IsOk());
  }

  for (size_t i = 0; i < slices_per_slab; i++) {
    size_t idx = (127 * i + 151) % slices_per_slab;
    ASSERT_THAT(slab.FreeSlice(static_cast<AllocatedSlice*>(slab.SliceAt(
                    SliceId<typename TypeParam::IdType>::FromIdx(idx)))),
                IsOk());
  }
}

TYPED_TEST(SmallSlabTest, FillUpThenEmptyAndRefill) {
  auto slab = SmallSlabTest<TypeParam>::MakeSlab();
  const uint64_t slices_per_slab =
      slab.Metadata().SizeClass().MaxSlicesPerSlab();

  for (size_t i = 0; i < slices_per_slab; i++) {
    ASSERT_THAT(slab.AllocSlice().status(), IsOk());
  }

  for (size_t i = 0; i < slices_per_slab / 3; i++) {
    size_t idx = (151 * i + 127) % slices_per_slab;
    ASSERT_THAT(slab.FreeSlice(static_cast<AllocatedSlice*>(slab.SliceAt(
                    SliceId<typename TypeParam::IdType>::FromIdx(idx)))),
                IsOk());
  }

  for (size_t i = 0; i < slices_per_slab / 3; i++) {
    ASSERT_THAT(slab.AllocSlice().status(), IsOk());
  }
}

}  // namespace ckmalloc

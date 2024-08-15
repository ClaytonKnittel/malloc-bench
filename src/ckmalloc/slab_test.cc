#include "src/ckmalloc/slab.h"

#include <array>
#include <cinttypes>
#include <cstdint>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
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

using SliceId16 = SliceId<uint16_t>;

class TestSmallSlab {
 public:
  explicit TestSmallSlab(SizeClass size_class) {
    static_cast<Slab*>(&slab_)->Init<SmallSlab>(
        PageId::Zero(), /*n_pages=*/UINT32_C(1), size_class);
  }

  SmallSlab& Underlying() {
    return slab_;
  }

  Slice* SliceAt(SliceId16 slice_id) {
    CK_ASSERT_NE(slice_id, SliceId16::Nil());
    return reinterpret_cast<Slice*>(
        &data_[slice_id.SliceOffsetBytes() / sizeof(uint64_t)]);
  }

  SliceId16 IdForSlice(Slice* slice) {
    return SliceId16(PtrDistance(slice, data_.data()));
  }

  absl::StatusOr<AllocatedSlice*> AllocSlice() {
    const SizeClass size_class = slab_.Metadata().SizeClass();
    CK_ASSERT_LT(allocated_slices_.size(), size_class.MaxSlicesPerSlab());

    AllocatedSlice* slice = slab_.Metadata().PopSlice(data_.data());

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
      if (!slab_.Metadata().Full()) {
        return absl::FailedPreconditionError(
            absl::StrFormat("Expected slab to be full with %" PRIu64
                            " allocations, but Full() returned false",
                            allocated_slices_.size()));
      }
    } else if (slab_.Metadata().Full()) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Expected slab to be non-full with %" PRIu64
                          " allocations, but Full() returned true",
                          allocated_slices_.size()));
    }

    if (slab_.Metadata().Empty()) {
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
    slab_.Metadata().PushSlice(data_.data(), slice->ToFree<uint16_t>());

    if (allocated_slices_.empty()) {
      if (!slab_.Metadata().Empty()) {
        return absl::FailedPreconditionError(
            absl::StrFormat("Expected slab to be empty with %" PRIu64
                            " allocations, but Empty() returned false",
                            allocated_slices_.size()));
      }
    } else if (slab_.Metadata().Empty()) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Expected slab to be non-empty with %" PRIu64
                          " allocations, but Empty() returned true",
                          allocated_slices_.size()));
    }

    if (slab_.Metadata().Full()) {
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
         i < slab_.Metadata().SizeClass().SliceSize() / sizeof(uint64_t); i++) {
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
         i < slab_.Metadata().SizeClass().SliceSize() / sizeof(uint64_t); i++) {
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

util::Rng TestSmallSlab::rng_ = util::Rng(1031, 5);

class SmallSlabTest : public ::testing::TestWithParam<uint64_t> {
 public:
  static SizeClass GetSizeClass() {
    return SizeClass::FromSliceSize(GetParam());
  }

  static TestSmallSlab MakeSlab() {
    return TestSmallSlab(GetSizeClass());
  }

 private:
  SmallSlab small_slab_;
};

TEST_P(SmallSlabTest, EmptySmallSlab) {
  TestSmallSlab slab = MakeSlab();
}

TEST_P(SmallSlabTest, SingleAllocation) {
  TestSmallSlab slab = MakeSlab();
  ASSERT_OK_AND_DEFINE(AllocatedSlice*, slice, slab.AllocSlice());
  EXPECT_EQ(slice, slab.SliceAt(SliceId16(0)));
}

TEST_P(SmallSlabTest, SingleFree) {
  TestSmallSlab slab = MakeSlab();
  ASSERT_OK_AND_DEFINE(AllocatedSlice*, slice, slab.AllocSlice());
  EXPECT_THAT(slab.FreeSlice(slice), IsOk());
}

TEST_P(SmallSlabTest, AllAllocations) {
  TestSmallSlab slab = MakeSlab();

  for (size_t i = 0; i < GetSizeClass().MaxSlicesPerSlab(); i++) {
    ASSERT_OK_AND_DEFINE(AllocatedSlice*, slice, slab.AllocSlice());
    ASSERT_EQ(slice, slab.SliceAt(SliceId16(i * GetSizeClass().SliceSize())));
  }
}

TEST_P(SmallSlabTest, FillUpThenEmpty) {
  TestSmallSlab slab = MakeSlab();

  for (size_t i = 0; i < GetSizeClass().MaxSlicesPerSlab(); i++) {
    ASSERT_THAT(slab.AllocSlice().status(), IsOk());
  }

  for (size_t i = 0; i < GetSizeClass().MaxSlicesPerSlab(); i++) {
    ASSERT_THAT(slab.FreeSlice(static_cast<AllocatedSlice*>(
                    slab.SliceAt(SliceId16(i * GetSizeClass().SliceSize())))),
                IsOk());
  }
}

TEST_P(SmallSlabTest, FillUpThenEmptyStrangeOrder) {
  TestSmallSlab slab = MakeSlab();

  for (size_t i = 0; i < GetSizeClass().MaxSlicesPerSlab(); i++) {
    ASSERT_THAT(slab.AllocSlice().status(), IsOk());
  }

  for (size_t i = 0; i < GetSizeClass().MaxSlicesPerSlab(); i++) {
    size_t idx = (127 * i + 151) % GetSizeClass().MaxSlicesPerSlab();
    ASSERT_THAT(slab.FreeSlice(static_cast<AllocatedSlice*>(
                    slab.SliceAt(SliceId16(idx * GetSizeClass().SliceSize())))),
                IsOk());
  }
}

TEST_P(SmallSlabTest, FillUpThenEmptyAndRefill) {
  TestSmallSlab slab = MakeSlab();

  for (size_t i = 0; i < GetSizeClass().MaxSlicesPerSlab(); i++) {
    ASSERT_THAT(slab.AllocSlice().status(), IsOk());
  }

  for (size_t i = 0; i < GetSizeClass().MaxSlicesPerSlab() / 3; i++) {
    size_t idx = (151 * i + 127) % GetSizeClass().MaxSlicesPerSlab();
    ASSERT_THAT(slab.FreeSlice(static_cast<AllocatedSlice*>(
                    slab.SliceAt(SliceId16(idx * GetSizeClass().SliceSize())))),
                IsOk());
  }

  for (size_t i = 0; i < GetSizeClass().MaxSlicesPerSlab() / 3; i++) {
    ASSERT_THAT(slab.AllocSlice().status(), IsOk());
  }
}

INSTANTIATE_TEST_SUITE_P(SlabTests, SmallSlabTest,
                         testing::Values(8, 16, 32, 48, 64, 80, 96, 112, 128),
                         [](const testing::TestParamInfo<uint64_t>& info) {
                           return absl::StrCat("SizeClass", info.param);
                         });

}  // namespace ckmalloc

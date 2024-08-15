#include "src/ckmalloc/slab.h"

#include <array>
#include <cstdint>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "util/absl_util.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/size_class.h"
#include "src/ckmalloc/slice.h"
#include "src/ckmalloc/util.h"
#include "src/rng.h"

namespace ckmalloc {

class TestSmallSlab {
 public:
  explicit TestSmallSlab(SizeClass size_class) {
    static_cast<Slab*>(&slab_)->Init<SmallSlab>(
        PageId::Zero(), /*n_pages=*/UINT32_C(1), size_class);
  }

  Slice* SliceAt(SliceId slice_id) {
    CK_ASSERT_NE(slice_id, SliceId::Nil());
    return reinterpret_cast<Slice*>(
        &data_[slice_id.SliceOffsetBytes(slab_.Metadata().SizeClass()) /
               sizeof(uint64_t)]);
  }

  SliceId IdForSlice(Slice* slice) {
    return SliceId(PtrDistance(slice, data_.data()) / kDefaultAlignment);
  }

  AllocatedSlice* AllocSlice() {
    AllocatedSlice* slice =
        slab_.Metadata().PopSlice([this](SliceId slice_id) -> class FreeSlice* {
          return static_cast<class FreeSlice*>(SliceAt(slice_id));
        });

    return slice;
  }

  absl::Status FreeSlice(AllocatedSlice* slice) {
    RETURN_IF_ERROR(CheckMagic(slice));

    slab_.Metadata().PushSlice(
        slice->ToFree(), IdForSlice(slice),
        [this](SliceId slice_id) -> class FreeSlice* {
          return static_cast<class FreeSlice*>(SliceAt(slice_id));
        });
  }

 private:
  absl::Status CheckMagic(AllocatedSlice* slice) {}

  static util::Rng rng_;

  SmallSlab slab_;
  std::array<uint64_t, kPageSize / sizeof(uint64_t)> data_;
  std::array<uint64_t, kPageSize / sizeof(uint64_t)> magic_;

  absl::flat_hash_set<AllocatedSlice*> allocated_slices_;
};

util::Rng TestSmallSlab::rng_ = util::Rng(1031, 5);

class SmallSlabTest : public ::testing::Test {
 public:
  static void MakeSlab(SizeClass size_class) {}

 private:
  SmallSlab small_slab_;
};

TEST_F(SmallSlabTest, EmptySmallSlab) {}

}  // namespace ckmalloc

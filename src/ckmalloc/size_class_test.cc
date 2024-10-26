#include "src/ckmalloc/size_class.h"

#include <cstddef>
#include <cstdint>

#include "gtest/gtest.h"

#include "src/ckmalloc/common.h"

namespace ckmalloc {

class SizeClassTest : public ::testing::Test {};

TEST_F(SizeClassTest, TestNil) {
  EXPECT_EQ(SizeClass(), SizeClass::Nil());
}

TEST_F(SizeClassTest, TestSizes) {
  size_t prev_size = 0;
  size_t ord = 0;
  for (const SizeClass::SizeClassInfo& info : SizeClass::kSizeClassInfo) {
    size_t user_size = info.max_size;
    EXPECT_EQ(SizeClass::FromUserDataSize(prev_size + 1),
              SizeClass::FromOrdinal(ord));
    EXPECT_EQ(SizeClass::FromUserDataSize(user_size),
              SizeClass::FromOrdinal(ord));
    ord++;
    prev_size = user_size;
  }
}

TEST_F(SizeClassTest, FromUserDataSize) {
  size_t ord = 0;
  for (size_t user_size = 1; user_size <= kMaxSmallSize; user_size++) {
    if (user_size > SizeClass::kSizeClassInfo[ord].max_size) {
      ord++;
      ASSERT_LT(ord, SizeClass::kNumSizeClasses);
    }
    EXPECT_EQ(SizeClass::FromUserDataSize(user_size),
              SizeClass::FromOrdinal(ord));
  }
}

TEST_F(SizeClassTest, NilIsDistinct) {
  for (size_t ord = 0; ord < SizeClass::kNumSizeClasses; ord++) {
    EXPECT_NE(SizeClass::FromOrdinal(ord), SizeClass::Nil());
  }
}

TEST_F(SizeClassTest, MaxSlicesPerSlab) {
  for (const SizeClass::SizeClassInfo& info : SizeClass::kSizeClassInfo) {
    size_t slice_size = info.max_size;
    size_t n_slices = info.pages * kPageSize / info.max_size;
    EXPECT_EQ(SizeClass::FromSliceSize(slice_size).MaxSlicesPerSlab(),
              n_slices);
  }
}

TEST_F(SizeClassTest, OffsetToIdx) {
  for (const SizeClass::SizeClassInfo& info : SizeClass::kSizeClassInfo) {
    size_t slice_size = info.max_size;
    SizeClass size_class = SizeClass::FromSliceSize(slice_size);
    for (size_t slice_idx = 0; slice_idx < size_class.MaxSlicesPerSlab();
         slice_idx++) {
      uint64_t offset_bytes = slice_idx * size_class.SliceSize();
      EXPECT_EQ(size_class.OffsetToIdx(offset_bytes), slice_idx);
    }
  }
}

TEST_F(SizeClassTest, TestAlignedSizes) {
  for (uint64_t ord = 0; ord < SizeClass::kNumSizeClasses; ord++) {
    SizeClass size_class = SizeClass::FromOrdinal(ord);
    for (size_t alignment = 1; alignment <= kMaxSmallSize; alignment *= 2) {
      SizeClass aligned_size_class =
          SizeClass::FromSliceSize(size_class.SliceSize(), alignment);

      uint64_t aligned_ord = ord;
      for (; aligned_ord < SizeClass::kNumSizeClasses &&
             SizeClass::FromOrdinal(aligned_ord).SliceSize() % alignment != 0;
           aligned_ord++)
        ;
      ASSERT_NE(aligned_ord, SizeClass::kNumSizeClasses);
      SizeClass expected_aligned_size = SizeClass::FromOrdinal(aligned_ord);
      EXPECT_EQ(aligned_size_class, expected_aligned_size)
          << "Expected equality for size " << size_class << " with alignment "
          << alignment;
    }
  }
}

}  // namespace ckmalloc

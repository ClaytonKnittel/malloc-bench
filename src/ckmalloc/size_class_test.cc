#include "src/ckmalloc/size_class.h"

#include <cstddef>

#include "gtest/gtest.h"

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

}  // namespace ckmalloc

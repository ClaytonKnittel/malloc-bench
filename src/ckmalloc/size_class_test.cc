#include "src/ckmalloc/size_class.h"

#include "gtest/gtest.h"

namespace ckmalloc {

class SizeClassTest : public ::testing::Test {};

TEST_F(SizeClassTest, TestNil) {
  EXPECT_EQ(SizeClass(), SizeClass::Nil());
}

TEST_F(SizeClassTest, TestSizes) {
  EXPECT_EQ(SizeClass::FromUserDataSize(1), SizeClass::FromOrdinal(0));
  EXPECT_EQ(SizeClass::FromUserDataSize(8), SizeClass::FromOrdinal(0));
  EXPECT_EQ(SizeClass::FromUserDataSize(9), SizeClass::FromOrdinal(1));
  EXPECT_EQ(SizeClass::FromUserDataSize(16), SizeClass::FromOrdinal(1));
  EXPECT_EQ(SizeClass::FromUserDataSize(17), SizeClass::FromOrdinal(2));
  EXPECT_EQ(SizeClass::FromUserDataSize(32), SizeClass::FromOrdinal(2));
}

}  // namespace ckmalloc

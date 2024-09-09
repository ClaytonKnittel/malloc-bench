#include "src/jsmalloc/util/bitset.h"

#include "gtest/gtest.h"

namespace jsmalloc {

TEST(BitSetTest, SetAndTest) {
  auto b = MakeBitSet<10>();
  EXPECT_EQ(b->test(0), false);
  b->set(0, true);
  EXPECT_EQ(b->test(0), true);
  b->set(0, false);
  EXPECT_EQ(b->test(0), false);
}

TEST(BitSetTest, CountTrailingOnes) {
  auto b = MakeBitSet<10>();
  EXPECT_EQ(b->countr_one(), 0);
  b->set(0, true);
  EXPECT_EQ(b->countr_one(), 1);
}

TEST(BitSet4096Test, SetAndTest) {
  auto b = MakeBitSet<128>();
  EXPECT_EQ(b->test(0), false);
  b->set(0, true);
  EXPECT_EQ(b->test(0), true);
  b->set(0, false);
  EXPECT_EQ(b->test(0), false);
}

TEST(BitSet4096Test, CountTrailingOnesBasic) {
  auto b = MakeBitSet<200>();
  EXPECT_EQ(b->countr_one(), 0);
  b->set(0, true);
  EXPECT_EQ(b->countr_one(), 1);
}

TEST(BitSet512Test, CountTrailingOnesAcrossMultipleLevels) {
  auto b = MakeBitSet<200>();
  for (int i = 0; i < 200; i++) {
    b->set(i, true);
    EXPECT_EQ(b->countr_one(), i + 1);
  }

  for (int i = 200 - 1; i >= 0; i--) {
    b->set(i, false);
    EXPECT_EQ(b->countr_one(), i);
  }
}

TEST(BitSet512Test, CountTrailingOnesSparse) {
  auto b = MakeBitSet<200>();
  for (int i = 0; i < 200; i++) {
    b->set(i, true);
  }

  b->set(66, false);
  EXPECT_EQ(b->countr_one(), 66);
}

}  // namespace jsmalloc

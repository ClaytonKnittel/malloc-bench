#include "src/jsmalloc/util/bitset.h"

#include "gtest/gtest.h"

namespace jsmalloc {

TEST(BitSetTest, SetAndTest) {
  BitSet<10> b;
  EXPECT_EQ(b.test(0), false);
  b.set(0, true);
  EXPECT_EQ(b.test(0), true);
  b.set(0, false);
  EXPECT_EQ(b.test(0), false);
}

TEST(BitSetTest, CountTrailingOnes) {
  BitSet<10> b;
  EXPECT_EQ(b.countr_one(), 0);
  b.set(0, true);
  EXPECT_EQ(b.countr_one(), 1);
}

TEST(BitSet4096Test, SetAndTest) {
  BitSet<200> b;
  EXPECT_EQ(b.test(0), false);
  b.set(0, true);
  EXPECT_EQ(b.test(0), true);
  b.set(0, false);
  EXPECT_EQ(b.test(0), false);
}

TEST(BitSet4096Test, CountTrailingOnesBasic) {
  BitSet4096<200> b;
  EXPECT_EQ(b.countr_one(), 0);
  b.set(0, true);
  EXPECT_EQ(b.countr_one(), 1);
}

TEST(BitSet512Test, CountTrailingOnesAcrossMultipleLevels) {
  BitSet4096<200> b;
  for (int i = 0; i < 200; i++) {
    b.set(i, true);
    EXPECT_EQ(b.countr_one(), i + 1);
  }

  for (int i = 200 - 1; i >= 0; i--) {
    b.set(i, false);
    EXPECT_EQ(b.countr_one(), i);
  }
}

TEST(BitSet512Test, CountTrailingOnesSparse) {
  BitSet4096<200> b;
  for (int i = 0; i < 200; i++) {
    b.set(i, true);
  }

  b.set(66, false);
  EXPECT_EQ(b.countr_one(), 66);
}

}  // namespace jsmalloc

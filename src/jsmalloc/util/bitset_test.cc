#include "src/jsmalloc/util/bitset.h"

#include "gtest/gtest.h"

#include "src/jsmalloc/util/allocable.h"

namespace jsmalloc {

TEST(BitSetTest, SetAndTest) {
  auto b = MakeAllocable<BitSet64>(10);
  EXPECT_EQ(b->test(0), false);
  b->set(0, true);
  EXPECT_EQ(b->test(0), true);
  b->set(0, false);
  EXPECT_EQ(b->test(0), false);
}

TEST(BitSetTest, CountTrailingOnes) {
  auto b = MakeAllocable<BitSet64>(10);
  EXPECT_EQ(b->countr_one(), 0);
  b->set(0, true);
  EXPECT_EQ(b->countr_one(), 1);
}

TEST(BitSet4096Test, SetAndTest) {
  auto b = MakeAllocable<BitSet4096>(200);
  EXPECT_EQ(b->test(0), false);
  b->set(0, true);
  EXPECT_EQ(b->test(0), true);
  b->set(0, false);
  EXPECT_EQ(b->test(0), false);
}

TEST(BitSet4096Test, CountTrailingOnesBasic) {
  auto b = MakeAllocable<BitSet4096>(200);
  EXPECT_EQ(b->countr_one(), 0);
  b->set(0, true);
  EXPECT_EQ(b->countr_one(), 1);
}

TEST(BitSet4096Test, CountTrailingOnesAcrossMultipleLevels) {
  auto b = MakeAllocable<BitSet4096>(200);
  for (int i = 0; i < 200; i++) {
    b->set(i, true);
    EXPECT_EQ(b->countr_one(), i + 1);
  }

  for (int i = 200 - 1; i >= 0; i--) {
    b->set(i, false);
    EXPECT_EQ(b->countr_one(), i);
  }
}

TEST(BitSet4096Test, CountTrailingOnesSparse) {
  auto b = MakeAllocable<BitSet4096>(200);
  for (int i = 0; i < 200; i++) {
    b->set(i, true);
  }

  b->set(66, false);
  EXPECT_EQ(b->countr_one(), 66);
}

TEST(BitSet512Test, SetAndTest) {
  auto b = MakeAllocable<BitSet512>(200);
  EXPECT_EQ(b->test(0), false);
  b->set(0, true);
  EXPECT_EQ(b->test(0), true);
  b->set(0, false);
  EXPECT_EQ(b->test(0), false);
}

TEST(BitSet512Test, CountTrailingOnesBasic) {
  auto b = MakeAllocable<BitSet512>(200);
  EXPECT_EQ(b->countr_one(), 0);
  b->set(0, true);
  EXPECT_EQ(b->popcount(), 1);
  EXPECT_EQ(b->countr_one(), 1);
}

}  // namespace jsmalloc

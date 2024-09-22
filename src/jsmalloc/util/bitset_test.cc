#include "src/jsmalloc/util/bitset.h"

#include "gtest/gtest.h"

#include "src/jsmalloc/util/allocable.h"

namespace jsmalloc {

TEST(BitSet64Test, SetAndTest) {
  auto b = MakeAllocable<BitSet64>(10);
  EXPECT_EQ(b->Test(0), false);
  b->Set(0, true);
  EXPECT_EQ(b->Test(0), true);
  b->Set(0, false);
  EXPECT_EQ(b->Test(0), false);
}

TEST(BitSet64Test, FindFirstUnsetBit) {
  auto b = MakeAllocable<BitSet64>(10);
  EXPECT_EQ(b->FindFirstUnsetBitFrom(8), 8);
  b->Set(8, true);
  EXPECT_EQ(b->FindFirstUnsetBitFrom(8), 9);
}

TEST(BitSet64Test, FindFirstUnsetBitFrom) {
  auto b = MakeAllocable<BitSet64>(10);
  EXPECT_EQ(b->FindFirstUnsetBit(), 0);
  b->Set(0, true);
  EXPECT_EQ(b->FindFirstUnsetBit(), 1);
}

TEST(BitSet4096Test, SetAndTest) {
  auto b = MakeAllocable<BitSet4096>(200);
  EXPECT_EQ(b->Test(0), false);
  b->Set(0, true);
  EXPECT_EQ(b->Test(0), true);
  b->Set(0, false);
  EXPECT_EQ(b->Test(0), false);
}

TEST(BitSet4096Test, FindFirstUnsetBitBasic) {
  auto b = MakeAllocable<BitSet4096>(200);
  EXPECT_EQ(b->FindFirstUnsetBit(), 0);
  b->Set(0, true);
  EXPECT_EQ(b->FindFirstUnsetBit(), 1);
}

TEST(BitSet4096Test, FindFirstUnsetBitAcrossMultipleLevels) {
  auto b = MakeAllocable<BitSet4096>(200);
  for (int i = 0; i < 200; i++) {
    b->Set(i, true);
    EXPECT_EQ(b->FindFirstUnsetBit(), i + 1);
  }

  for (int i = 200 - 1; i >= 0; i--) {
    b->Set(i, false);
    EXPECT_EQ(b->FindFirstUnsetBit(), i);
  }
}

TEST(BitSet4096Test, FindFirstUnsetBitFromSingleBit) {
  auto b = MakeAllocable<BitSet4096>(200);

  EXPECT_EQ(b->FindFirstUnsetBitFrom(100), 100);
  b->Set(100);
  EXPECT_EQ(b->FindFirstUnsetBitFrom(100), 101);
}

TEST(BitSet4096Test, FindFirstUnsetBitFromAcrossMultipleLevels) {
  auto b = MakeAllocable<BitSet4096>(200);

  EXPECT_EQ(b->FindFirstUnsetBitFrom(100), 100);
  b->Set(100);
  b->Set(101);
  EXPECT_EQ(b->FindFirstUnsetBitFrom(100), 102);

  b->SetRange(10, 100);
  EXPECT_EQ(b->FindFirstUnsetBitFrom(90), 102);
}

TEST(BitSet4096Test, FindFirstUnsetBitFromEdges) {
  auto b = MakeAllocable<BitSet4096>(200);

  EXPECT_EQ(b->FindFirstUnsetBitFrom(200), 200);
  EXPECT_EQ(b->FindFirstUnsetBitFrom(0), 0);
}

TEST(BitSet4096Test, FindFirstUnsetBitSparse) {
  auto b = MakeAllocable<BitSet4096>(200);
  for (int i = 0; i < 200; i++) {
    b->Set(i, true);
  }

  b->Set(66, false);
  EXPECT_EQ(b->FindFirstUnsetBit(), 66);
}

TEST(BitSet512Test, SetAndTest) {
  auto b = MakeAllocable<BitSet512>(200);
  EXPECT_EQ(b->Test(0), false);
  b->Set(0, true);
  EXPECT_EQ(b->Test(0), true);
  b->Set(0, false);
  EXPECT_EQ(b->Test(0), false);
}

TEST(BitSet512Test, FindFirstUnsetBitBasic) {
  auto b = MakeAllocable<BitSet512>(200);
  EXPECT_EQ(b->FindFirstUnsetBit(), 0);
  b->Set(0, true);
  EXPECT_EQ(b->PopCount(), 1);
  EXPECT_EQ(b->FindFirstUnsetBit(), 1);
}

TEST(BitSet, StaticallyAllocated) {
  BitSet<201> b;
  EXPECT_FALSE(b.Test(200));
  b.Set(200, true);
  EXPECT_TRUE(b.Test(200));
}

TEST(BitSet262144Test, FindFirstUnsetBit) {
  BitSet<260000> b;
  b.SetRange(0, 200000);
  EXPECT_EQ(b.FindFirstUnsetBit(), 200000);

  b.Set(0, false);
  EXPECT_EQ(b.FindFirstUnsetBit(), 0);
  EXPECT_EQ(b.FindFirstUnsetBitFrom(1), 200000);
}

}  // namespace jsmalloc

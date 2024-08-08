#include "src/jsmalloc/block.h"

#include <vector>

#include "gtest/gtest.h"

#include "src/jsmalloc/mallocator.h"

namespace jsmalloc {

TEST(TestSmallBlock, FullLifecycle) {
  BigStackMallocator mallactor;
  SmallBlock* block =
      SmallBlock::New(&mallactor, { .data_size = 12, .bin_count = 32 });

  EXPECT_TRUE(block->IsFree());

  std::vector<void*> ptrs;
  while (block->CanAlloc()) {
    ptrs.push_back(block->Alloc());
  }

  EXPECT_EQ(ptrs.size(), 32);

  for (void* ptr : ptrs) {
    EXPECT_FALSE(block->IsFree());
    block->Free(ptr);
    EXPECT_TRUE(block->CanAlloc());
  }

  EXPECT_TRUE(block->IsFree());
}

TEST(TestSmallBlock, ReportsSize) {
  BigStackMallocator mallactor;
  SmallBlock* block =
      SmallBlock::New(&mallactor, { .data_size = 12, .bin_count = 32 });

  EXPECT_GT(block->Size(), 12 * 32);
  EXPECT_EQ(block->DataSize(), 12);
}

TEST(TestSmallBlock, FromDataPointer) {
  BigStackMallocator mallactor;
  SmallBlock* block =
      SmallBlock::New(&mallactor, { .data_size = 12, .bin_count = 20 });

  std::vector<void*> ptrs;
  while (block->CanAlloc()) {
    ptrs.push_back(block->Alloc());
  }

  EXPECT_EQ(ptrs.size(), 20);

  for (void* ptr : ptrs) {
    EXPECT_EQ(static_cast<void*>(BlockFromDataPointer(ptr)),
              static_cast<void*>(block));
  }
}

TEST(TestLargeBlock, FullLifecycle) {
  BigStackMallocator mallocator;
  LargeBlock* block = LargeBlock::New(&mallocator, 50);

  EXPECT_TRUE(block->IsFree());
  EXPECT_TRUE(block->CanAlloc());

  void* ptr = block->Alloc();
  EXPECT_FALSE(block->IsFree());
  EXPECT_FALSE(block->CanAlloc());

  auto* block_from_ptr =
      reinterpret_cast<LargeBlock*>(BlockFromDataPointer(ptr));
  EXPECT_EQ(block, block_from_ptr);

  block->Free(ptr);
  EXPECT_TRUE(block->IsFree());
  EXPECT_TRUE(block->CanAlloc());
}

TEST(TestLargeBlock, ComputesSize) {
  BigStackMallocator mallocator;
  LargeBlock* block = LargeBlock::New(&mallocator, 100);

  EXPECT_GT(block->Size(), 100);
  EXPECT_LT(block->Size(), 200);
}

}  // namespace jsmalloc

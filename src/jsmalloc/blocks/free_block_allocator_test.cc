#include "src/jsmalloc/blocks/free_block_allocator.h"

#include <cstdint>

#include "gtest/gtest.h"

#include "src/jsmalloc/allocator.h"
#include "src/jsmalloc/blocks/free_block.h"

namespace jsmalloc {
namespace blocks {

// Minimum size that FreeBlockAllocator will bother saving blocks for.
constexpr size_t kMinSavedBlockSize = 256;

TEST(TestFreeBlockAllocator, AllocatesExistingBlocks) {
  BigStackAllocator stack_allocator;
  FreeBlockAllocator allocator(stack_allocator);

  FreeBlock* b1 = allocator.Allocate(kMinSavedBlockSize);
  FreeBlock* b2 = allocator.Allocate(kMinSavedBlockSize + 16);
  FreeBlock* b3 = allocator.Allocate(kMinSavedBlockSize + 32);

  allocator.Free(b1->Header());
  EXPECT_EQ(b1, allocator.Allocate(kMinSavedBlockSize));

  allocator.Free(b2->Header());
  EXPECT_EQ(b2, allocator.Allocate(kMinSavedBlockSize + 16));

  allocator.Free(b3->Header());
  EXPECT_EQ(b3, allocator.Allocate(kMinSavedBlockSize + 32));
}

TEST(TestFreeBlockAllocator, SplitsBlocks) {
  BigStackAllocator stack_allocator;
  FreeBlockAllocator allocator(stack_allocator);

  FreeBlock* b = allocator.Allocate(kMinSavedBlockSize * 3);
  allocator.Free(b->Header());

  FreeBlock* b1 = allocator.Allocate(kMinSavedBlockSize);
  FreeBlock* b2 = allocator.Allocate(kMinSavedBlockSize);

  EXPECT_EQ(b, b1);
  EXPECT_EQ(reinterpret_cast<uint8_t*>(b) + kMinSavedBlockSize,
            reinterpret_cast<uint8_t*>(b2));
}

}  // namespace blocks
}  // namespace jsmalloc

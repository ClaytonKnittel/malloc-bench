#include "src/jsmalloc/blocks/free_block_allocator.h"

#include <cstdint>

#include "gtest/gtest.h"

#include "src/jsmalloc/allocator.h"
#include "src/jsmalloc/blocks/free_block.h"

namespace jsmalloc {
namespace blocks {

TEST(TestFreeBlockAllocator, AllocatesExistingBlocks) {
  BigStackAllocator stack_allocator;
  FreeBlockAllocator allocator(stack_allocator);

  FreeBlock* b1 = allocator.Allocate(48);
  FreeBlock* b2 = allocator.Allocate(64);
  FreeBlock* b3 = allocator.Allocate(80);

  allocator.Free(b1->Header());
  EXPECT_EQ(b1, allocator.Allocate(48));

  allocator.Free(b2->Header());
  EXPECT_EQ(b2, allocator.Allocate(64));

  allocator.Free(b3->Header());
  EXPECT_EQ(b3, allocator.Allocate(80));
}

TEST(TestFreeBlockAllocator, SplitsBlocks) {
  BigStackAllocator stack_allocator;
  FreeBlockAllocator allocator(stack_allocator);

  FreeBlock* b = allocator.Allocate(128);
  allocator.Free(b->Header());

  FreeBlock* b1 = allocator.Allocate(48);
  FreeBlock* b2 = allocator.Allocate(80);

  EXPECT_EQ(b, b1);
  EXPECT_EQ(reinterpret_cast<uint8_t*>(b) + 48, reinterpret_cast<uint8_t*>(b2));
}

}  // namespace blocks
}  // namespace jsmalloc

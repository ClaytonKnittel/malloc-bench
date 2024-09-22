#include "src/jsmalloc/blocks/small_block_allocator.h"

#include <cstdint>

#include "gtest/gtest.h"

namespace jsmalloc {
namespace blocks {

/** Returns the size class that `data_size` belongs to. */
static constexpr uint32_t SizeClassBruteForce(uint32_t data_size) {
  for (uint32_t cls = 0; cls < SmallBlockAllocator::kSizeClasses; cls++) {
    if (data_size <= SmallBlockAllocator::kMaxDataSizePerSizeClass[cls]) {
      return cls;
    }
  }
  std::terminate();
}

TEST(SmallBlockAllocatorTest, CheckSizeClassMatchesBruteForce) {
  for (uint32_t data_size = 1; data_size <= SmallBlockAllocator::kMaxDataSize;
       data_size++) {
    EXPECT_EQ(SizeClassBruteForce(data_size), small::SizeClass(data_size))
        << " for data_size=" << data_size;
  }
}

}  // namespace blocks
}  // namespace jsmalloc

#include "src/ckmalloc/freelist.h"

#include "gtest/gtest.h"

#include "src/ckmalloc/block.h"

namespace ckmalloc {

class FreelistTest : public ::testing::Test {
 public:
  Freelist& Freelist() {
    return freelist_;
  }

 private:
  class Freelist freelist_;
};

TEST_F(FreelistTest, Empty) {
  EXPECT_EQ(Freelist().FindFree(0), nullptr);
}

}  // namespace ckmalloc

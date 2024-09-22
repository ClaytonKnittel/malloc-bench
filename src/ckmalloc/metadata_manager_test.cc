#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>

#include "absl/status/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "util/absl_util.h"
#include "util/gtest_util.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/metadata_manager_test_fixture.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/testlib.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

using testing::UnorderedElementsAreArray;
using util::IsOk;
using util::IsOkAndHolds;

class MetadataManagerTest : public testing::Test {
 public:
  static constexpr size_t kNumPages = 64;

  MetadataManagerTest()
      : heap_(std::make_shared<TestHeap>(kNumPages)),
        slab_map_(std::make_shared<TestSlabMap>()),
        metadata_manager_fixture_(
            std::make_shared<MetadataManagerFixture>(heap_, slab_map_)) {}

  bench::Heap& Heap() {
    return *heap_;
  }

  TestMetadataManager& MetadataManager() {
    return metadata_manager_fixture_->MetadataManager();
  }

  MetadataManagerFixture& Fixture() {
    return *metadata_manager_fixture_;
  }

  absl::StatusOr<size_t> SlabMetaFreelistLength() const {
    return metadata_manager_fixture_->SlabMetaFreelistLength();
  }

  absl::Status ValidateHeap() {
    RETURN_IF_ERROR(metadata_manager_fixture_->ValidateHeap());
    return absl::OkStatus();
  }

 private:
  std::shared_ptr<TestHeap> heap_;
  std::shared_ptr<TestSlabMap> slab_map_;
  std::shared_ptr<MetadataManagerFixture> metadata_manager_fixture_;
};

TEST_F(MetadataManagerTest, TestEmpty) {
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(SlabMetaFreelistLength(), IsOkAndHolds(0));
}

TEST_F(MetadataManagerTest, AllocateOnce) {
  ASSERT_OK_AND_DEFINE(void*, value, Fixture().Alloc(16));
  EXPECT_EQ(value, Heap().Start());
}

TEST_F(MetadataManagerTest, AllocateAdjacent) {
  ASSERT_OK_AND_DEFINE(void*, v1, Fixture().Alloc(7));
  ASSERT_OK_AND_DEFINE(void*, v2, Fixture().Alloc(41));
  ASSERT_OK_AND_DEFINE(void*, v3, Fixture().Alloc(60));

  EXPECT_EQ(PtrDistance(v2, v1), 7);
  EXPECT_EQ(PtrDistance(v3, v2), 41);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MetadataManagerTest, AllocateAligned) {
  ASSERT_OK_AND_DEFINE(void*, v1, Fixture().Alloc(7));
  // Should range from 8 - 55 (inclusive)
  ASSERT_OK_AND_DEFINE(void*, v2, Fixture().Alloc(48, 8));
  // Should range from 64 - 127 (inclusive)
  ASSERT_OK_AND_DEFINE(void*, v3, Fixture().Alloc(64, 64));

  EXPECT_EQ(PtrDistance(v2, v1), 8);
  EXPECT_EQ(PtrDistance(v3, v2), 56);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MetadataManagerTest, AllocateLarge) {
  ASSERT_OK_AND_DEFINE(void*, value, Fixture().Alloc(kPageSize));
  EXPECT_EQ(value, Heap().Start());
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MetadataManagerTest, AllocateExtraLarge) {
  ASSERT_OK_AND_DEFINE(void*, value, Fixture().Alloc(11 * kPageSize));
  EXPECT_EQ(value, Heap().Start());
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MetadataManagerTest, AllocateAndStay) {
  ASSERT_THAT(Fixture().Alloc(kPageSize / 2).status(), IsOk());
  ASSERT_OK_AND_DEFINE(void*, v2, Fixture().Alloc(3 * kPageSize / 4));
  // v2 should be allocated in a new page by itself.
  EXPECT_EQ(v2, PtrAdd(Heap().Start(), kPageSize));
  EXPECT_THAT(ValidateHeap(), IsOk());

  // Since the remainder in the first slab was higher, it should continue to be
  // allocated from.
  ASSERT_OK_AND_DEFINE(void*, v3, Fixture().Alloc(kPageSize / 2));
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(v3, static_cast<uint8_t*>(Heap().Start()) + kPageSize / 2);
  EXPECT_EQ(Heap().Size(), 2 * kPageSize);
}

TEST_F(MetadataManagerTest, AllocateAndSwitch) {
  ASSERT_THAT(Fixture().Alloc(3 * kPageSize / 4).status(), IsOk());
  ASSERT_OK_AND_DEFINE(void*, v2, Fixture().Alloc(kPageSize / 2));
  // v2 should be allocated in a new page by itself.
  EXPECT_EQ(v2, PtrAdd(Heap().Start(), kPageSize));
  EXPECT_THAT(ValidateHeap(), IsOk());

  // Since the remainder in the second slab was higher, it should continue to be
  // allocated from.
  ASSERT_OK_AND_DEFINE(void*, v3, Fixture().Alloc(kPageSize / 2));
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(v3, static_cast<uint8_t*>(Heap().Start()) + 3 * kPageSize / 2);
  EXPECT_EQ(Heap().Size(), 2 * kPageSize);
}

TEST_F(MetadataManagerTest, AllocateLargeAndStay) {
  ASSERT_THAT(Fixture().Alloc(32).status(), IsOk());
  ASSERT_OK_AND_DEFINE(void*, v2, Fixture().Alloc(kPageSize + 64));
  // v2 should be allocated in a new slab by itself since it is so large.
  EXPECT_EQ(v2, PtrAdd(Heap().Start(), kPageSize));
  EXPECT_THAT(ValidateHeap(), IsOk());

  // Since the remainder in the first slab was higher, it should continue to be
  // allocated from.
  ASSERT_OK_AND_DEFINE(void*, v3, Fixture().Alloc(kPageSize - 32));
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(v3, static_cast<uint8_t*>(Heap().Start()) + 32);
  EXPECT_EQ(Heap().Size(), 3 * kPageSize);
}

TEST_F(MetadataManagerTest, AllocateLargeAndSwitch) {
  ASSERT_THAT(Fixture().Alloc(64).status(), IsOk());
  ASSERT_OK_AND_DEFINE(void*, v2, Fixture().Alloc(kPageSize + 32));
  // v2 should be allocated in a new slab by itself since it is so large.
  EXPECT_EQ(v2, PtrAdd(Heap().Start(), kPageSize));
  EXPECT_THAT(ValidateHeap(), IsOk());

  // Since the remainder in the second slab was higher, it should continue to be
  // allocated from.
  ASSERT_OK_AND_DEFINE(void*, v3, Fixture().Alloc(kPageSize - 32));
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(v3, static_cast<uint8_t*>(Heap().Start()) + 2 * kPageSize + 32);
  EXPECT_EQ(Heap().Size(), 3 * kPageSize);
}

TEST_F(MetadataManagerTest, AllocateSlabMeta) {
  ASSERT_OK_AND_DEFINE(Slab*, slab, Fixture().NewSlabMeta());
  EXPECT_EQ(slab, Heap().Start());
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MetadataManagerTest, AllocateSlabMetaTwice) {
  ASSERT_OK_AND_DEFINE(Slab*, s1, Fixture().NewSlabMeta());
  ASSERT_THAT(Fixture().FreeSlabMeta(s1), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());

  ASSERT_OK_AND_DEFINE(Slab*, s2, Fixture().NewSlabMeta());
  EXPECT_EQ(s2, Heap().Start());
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MetadataManagerTest, AllocateSlabMetaWithNormalAllocation) {
  static_assert(sizeof(Slab) > 1, "Test only meaningful if sizeof(Slab) > 1");

  ASSERT_THAT(Fixture().Alloc(kPageSize - 1).status(), IsOk());
  ASSERT_OK_AND_DEFINE(Slab*, s1, Fixture().NewSlabMeta());
  EXPECT_THAT(ValidateHeap(), IsOk());
  // The new slab should have been placed at the beginning of the second page.
  EXPECT_EQ(s1, PtrAdd(Heap().Start(), kPageSize));
}

TEST_F(MetadataManagerTest, SlabMetaFreelistBeforeNewAlloc) {
  constexpr size_t kNumSlabs = 20;

  // Allocate a bunch of slabs.
  std::vector<Slab*> slabs;
  slabs.reserve(kNumSlabs);
  for (size_t i = 0; i < kNumSlabs; i++) {
    ASSERT_OK_AND_DEFINE(Slab*, slab, Fixture().NewSlabMeta());
    ASSERT_THAT(ValidateHeap(), IsOk());
    slabs.push_back(slab);
  }

  // Free all of the slabs in random order.
  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(slabs.begin(), slabs.end(), rd);

  for (Slab* slab : slabs) {
    ASSERT_THAT(Fixture().FreeSlabMeta(slab), IsOk());
  }

  // Allocate that many new slabs.
  std::vector<Slab*> new_slabs;
  new_slabs.reserve(kNumSlabs);
  for (size_t i = 0; i < kNumSlabs; i++) {
    ASSERT_OK_AND_DEFINE(Slab*, slab, Fixture().NewSlabMeta());
    ASSERT_THAT(ValidateHeap(), IsOk());
    new_slabs.push_back(slab);
  }

  // Every new allocation should have come from a previous allocation.
  EXPECT_THAT(new_slabs, UnorderedElementsAreArray(slabs));
}

TEST_F(MetadataManagerTest, InterleaveSlabAllocAndAlloc) {
  ASSERT_OK_AND_DEFINE(Slab*, s1, Fixture().NewSlabMeta());
  ASSERT_THAT(Fixture().FreeSlabMeta(s1), IsOk());

  ASSERT_OK_AND_DEFINE(void*, value,
                       Fixture().Alloc(sizeof(Slab), alignof(Slab)));
  EXPECT_NE(value, s1);

  ASSERT_OK_AND_DEFINE(Slab*, s2, Fixture().NewSlabMeta());
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(s2, s1);
}

}  // namespace ckmalloc

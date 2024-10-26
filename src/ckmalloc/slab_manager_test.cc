#include <cstddef>
#include <memory>
#include <ranges>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "util/gtest_util.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_manager_test_fixture.h"
#include "src/ckmalloc/sys_alloc.h"
#include "src/ckmalloc/testlib.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

using testing::_;
using testing::AllOf;
using testing::ElementsAre;
using testing::Pointee;
using testing::Property;
using util::IsOk;

class SlabManagerTest : public testing::Test {
 public:
  static constexpr size_t kHeapSize = 64 * kPageSize;

  SlabManagerTest()
      : heap_factory_(std::make_shared<TestHeapFactory>(kHeapSize)),
        slab_map_(std::make_shared<TestSlabMap>()),
        test_fixture_(heap_factory_, slab_map_, kHeapSize) {
    TestSysAlloc::NewInstance(heap_factory_.get());
  }

  ~SlabManagerTest() override {
    TestSysAlloc::Reset();
  }

  static auto Heaps() {
    return SlabManagerFixture::Heaps();
  }

  static std::vector<std::pair<void* const, std::pair<HeapType, TestHeap*>>>
  HeapsVec() {
    auto heaps = Heaps();
    return std::vector(heaps.begin(), heaps.end());
  }

  static size_t TotalHeapsSize() {
    return SlabManagerFixture::TotalHeapsSize();
  }

  TestSlabManager& SlabManager() {
    return test_fixture_.SlabManager();
  }

  SlabManagerFixture& Fixture() {
    return test_fixture_;
  }

  absl::Status ValidateHeap() {
    return test_fixture_.ValidateHeap();
  }

  absl::Status ValidateEmpty() {
    return test_fixture_.ValidateEmpty();
  }

 private:
  std::shared_ptr<TestHeapFactory> heap_factory_;
  std::shared_ptr<TestSlabMap> slab_map_;
  SlabManagerFixture test_fixture_;
};

TEST_F(SlabManagerTest, AllPtrsInFirstPageIdZero) {
  ASSERT_THAT(Fixture().AllocateSlab(1), IsOk());
  ASSERT_THAT(HeapsVec(), ElementsAre(_));

  const TestHeap& slab_heap = *Heaps().begin()->second.second;
  PageId start_id = PageId::FromPtr(slab_heap.Start());
  for (size_t offset = 0; offset < kPageSize; offset++) {
    EXPECT_EQ(PageId::FromPtr(PtrAdd(slab_heap.Start(), offset)), start_id);
  }
}

TEST_F(SlabManagerTest, PageIdIncreasesPerPage) {
  constexpr size_t kPages = 16;
  ASSERT_THAT(Fixture().AllocateSlab(kPages), IsOk());
  ASSERT_THAT(HeapsVec(), ElementsAre(_));

  const TestHeap& slab_heap = *Heaps().begin()->second.second;
  for (size_t page_n = 0; page_n < kPages; page_n++) {
    void* beginning = PtrAdd(slab_heap.Start(), page_n * kPageSize);
    void* end = PtrAdd(beginning, kPageSize - 1);
    EXPECT_EQ(PageId::FromPtr(beginning),
              PageId::FromPtr(slab_heap.Start()) + page_n);
    EXPECT_EQ(PageId::FromPtr(end),
              PageId::FromPtr(slab_heap.Start()) + page_n);
  }
}

TEST_F(SlabManagerTest, SlabStartFromId) {
  constexpr size_t kPages = 16;
  ASSERT_THAT(Fixture().AllocateSlab(kPages), IsOk());
  ASSERT_THAT(HeapsVec(), ElementsAre(_));

  const TestHeap& slab_heap = *Heaps().begin()->second.second;
  for (size_t page_n = 0; page_n < kPages; page_n++) {
    EXPECT_EQ((PageId::FromPtr(slab_heap.Start()) + page_n).PageStart(),
              PtrAdd(slab_heap.Start(), page_n * kPageSize));
  }
}

TEST_F(SlabManagerTest, EmptyHeapValid) {
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(SlabManagerTest, SinglePageHeapValid) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab, Fixture().AllocateSlab(1));
  ASSERT_THAT(HeapsVec(), ElementsAre(_));

  const TestHeap& slab_heap = *Heaps().begin()->second.second;
  EXPECT_EQ(slab->StartId(), PageId::FromPtr(slab_heap.Start()));
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, TwoAdjacentAllocatedSlabs) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab1, Fixture().AllocateSlab(1));
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab2, Fixture().AllocateSlab(1));
  ASSERT_THAT(HeapsVec(), ElementsAre(_));

  const TestHeap& slab_heap = *Heaps().begin()->second.second;
  EXPECT_EQ(slab1->StartId(), PageId::FromPtr(slab_heap.Start()));
  EXPECT_EQ(slab2->StartId(), PageId::FromPtr(slab_heap.Start()) + 1);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, SingleLargeSlab) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab, Fixture().AllocateSlab(9));
  ASSERT_THAT(HeapsVec(), ElementsAre(_));

  const TestHeap& slab_heap = *Heaps().begin()->second.second;
  EXPECT_EQ(slab->StartId(), PageId::FromPtr(slab_heap.Start()));
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, FreeOnce) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab, Fixture().AllocateSlab(1));
  ASSERT_THAT(Fixture().FreeSlab(slab), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(SlabManagerTest, FreeLarge) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab, Fixture().AllocateSlab(12));
  ASSERT_THAT(Fixture().FreeSlab(slab), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(SlabManagerTest, FreeTwice) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab1, Fixture().AllocateSlab(1));
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab2, Fixture().AllocateSlab(1));
  ASSERT_THAT(Fixture().FreeSlab(slab1), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  ASSERT_THAT(Fixture().FreeSlab(slab2), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(SlabManagerTest, CoalesceBehind) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab1, Fixture().AllocateSlab(3));
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab2, Fixture().AllocateSlab(5));
  ASSERT_THAT(Fixture().FreeSlab(slab2), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  ASSERT_THAT(Fixture().FreeSlab(slab1), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(SlabManagerTest, CoalesceAhead) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab1, Fixture().AllocateSlab(2));
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab2, Fixture().AllocateSlab(5));
  ASSERT_THAT(Fixture().FreeSlab(slab1), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  ASSERT_THAT(Fixture().FreeSlab(slab2), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(SlabManagerTest, CoalesceBothDirections) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab1, Fixture().AllocateSlab(2));
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab2, Fixture().AllocateSlab(1));
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab3, Fixture().AllocateSlab(3));
  ASSERT_THAT(Fixture().FreeSlab(slab1), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  ASSERT_THAT(Fixture().FreeSlab(slab3), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  ASSERT_THAT(Fixture().FreeSlab(slab2), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(SlabManagerTest, ReAllocateFreed) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab1, Fixture().AllocateSlab(2));
  ASSERT_THAT(Fixture().FreeSlab(slab1), IsOk());
  ASSERT_THAT(Fixture().AllocateSlab(1).status(), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(TotalHeapsSize(), kPageSize);
}

TEST_F(SlabManagerTest, ExtendHeapWithFreeAtEnd) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab1, Fixture().AllocateSlab(2));
  ASSERT_THAT(Fixture().FreeSlab(slab1), IsOk());
  ASSERT_THAT(Fixture().AllocateSlab(3).status(), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(TotalHeapsSize(), 3 * kPageSize);
}

TEST_F(SlabManagerTest, BestFit) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab1, Fixture().AllocateSlab(3));
  ASSERT_THAT(Fixture().AllocateSlab(1).status(), IsOk());
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab3, Fixture().AllocateSlab(6));
  ASSERT_THAT(Fixture().AllocateSlab(1).status(), IsOk());
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab5, Fixture().AllocateSlab(4));
  ASSERT_THAT(Fixture().AllocateSlab(1).status(), IsOk());
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab7, Fixture().AllocateSlab(8));
  PageId slab5_start = slab5->StartId();

  // Free all the larger slabs, which should have alternatively fill the heap.
  ASSERT_THAT(Fixture().FreeSlab(slab1), IsOk());
  ASSERT_THAT(Fixture().FreeSlab(slab3), IsOk());
  ASSERT_THAT(Fixture().FreeSlab(slab5), IsOk());
  ASSERT_THAT(Fixture().FreeSlab(slab7), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  // There should now be free slabs of size 3, 4, 6, and 8.

  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab8, Fixture().AllocateSlab(4));

  // We should have found the perfect fit, which used to be slab 5.
  EXPECT_EQ(slab8->StartId(), slab5_start);
  EXPECT_EQ(TotalHeapsSize(), 24 * kPageSize);
}

TEST_F(SlabManagerTest, SlabFillsWholeHeap) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab,
                       Fixture().AllocateSlab(kHeapSize / kPageSize));
  ASSERT_THAT(HeapsVec(), ElementsAre(_));

  const TestHeap& slab_heap = *Heaps().begin()->second.second;
  EXPECT_EQ(slab->StartId(), PageId::FromPtr(slab_heap.Start()));
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, TwoHeaps) {
  EXPECT_THAT(Fixture().AllocateSlab(kHeapSize / kPageSize).status(), IsOk());
  EXPECT_THAT(Fixture().AllocateSlab(2).status(), IsOk());
  ASSERT_THAT(HeapsVec(), ElementsAre(_, _));
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, FreeTwoHeaps) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab1,
                       Fixture().AllocateSlab(kHeapSize / kPageSize));
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab2, Fixture().AllocateSlab(2));
  EXPECT_THAT(Fixture().FreeSlab(slab1), IsOk());
  EXPECT_THAT(Fixture().FreeSlab(slab2), IsOk());

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(SlabManagerTest, ExtendHeapWithExtraRoom) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab1,
                       Fixture().AllocateSlab(kHeapSize / kPageSize - 5));
  EXPECT_THAT(Fixture().AllocateSlab(6).status(), IsOk());
  ASSERT_THAT(HeapsVec(), ElementsAre(_, _));

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(Fixture().SlabMap().FindSlab(slab1->EndId() + 1),
              Pointee(AllOf(Property(&Slab::Type, SlabType::kFree),
                            Property(&AllocatedSlab::Pages, 5))));
}

TEST_F(SlabManagerTest, SingleUnderalignedPage) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab,
                       Fixture().AllocateSlab(1, kPageSize));
  ASSERT_THAT(HeapsVec(), ElementsAre(_));

  const TestHeap& slab_heap = *Heaps().begin()->second.second;
  EXPECT_EQ(slab->StartId(), PageId::FromPtr(slab_heap.Start()));
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, SingleAlignedPage) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab,
                       Fixture().AllocateSlab(1, 64 * kPageSize));
  ASSERT_THAT(HeapsVec(), ElementsAre(_));

  const TestHeap& slab_heap = *Heaps().begin()->second.second;
  EXPECT_EQ(slab->StartId(), PageId::FromPtr(slab_heap.Start()));
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, TwoAdjacentAlignedSlabs) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab1,
                       Fixture().AllocateSlab(1, 4 * kPageSize));
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab2,
                       Fixture().AllocateSlab(1, 4 * kPageSize));
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab3, Fixture().AllocateSlab(3));
  ASSERT_THAT(HeapsVec(), ElementsAre(_));

  const TestHeap& slab_heap = *Heaps().begin()->second.second;
  EXPECT_EQ(slab1->StartId(), PageId::FromPtr(slab_heap.Start()));
  EXPECT_EQ(slab2->StartId(), PageId::FromPtr(slab_heap.Start()) + 4);
  EXPECT_EQ(slab3->StartId(), PageId::FromPtr(slab_heap.Start()) + 1);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, AlignedSlabBetweenSlabs) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab1,
                       Fixture().AllocateSlab(1, 4 * kPageSize));
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab2,
                       Fixture().AllocateSlab(1, 4 * kPageSize));
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab3,
                       Fixture().AllocateSlab(1, 2 * kPageSize));
  ASSERT_THAT(HeapsVec(), ElementsAre(_));

  const TestHeap& slab_heap = *Heaps().begin()->second.second;
  EXPECT_EQ(slab1->StartId(), PageId::FromPtr(slab_heap.Start()));
  EXPECT_EQ(slab2->StartId(), PageId::FromPtr(slab_heap.Start()) + 4);
  EXPECT_EQ(slab3->StartId(), PageId::FromPtr(slab_heap.Start()) + 2);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, AlignedSlabBetweenSlabs2) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab1,
                       Fixture().AllocateSlab(1, 4 * kPageSize));
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab2,
                       Fixture().AllocateSlab(1, 4 * kPageSize));
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab3,
                       Fixture().AllocateSlab(2, 2 * kPageSize));
  ASSERT_THAT(HeapsVec(), ElementsAre(_));

  const TestHeap& slab_heap = *Heaps().begin()->second.second;
  EXPECT_EQ(slab1->StartId(), PageId::FromPtr(slab_heap.Start()));
  EXPECT_EQ(slab2->StartId(), PageId::FromPtr(slab_heap.Start()) + 4);
  EXPECT_EQ(slab3->StartId(), PageId::FromPtr(slab_heap.Start()) + 2);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, AlignedSlabBetweenSlabsExactFit) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab1,
                       Fixture().AllocateSlab(2, 4 * kPageSize));
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab2,
                       Fixture().AllocateSlab(1, 4 * kPageSize));
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab3,
                       Fixture().AllocateSlab(2, 2 * kPageSize));
  ASSERT_THAT(HeapsVec(), ElementsAre(_));

  const TestHeap& slab_heap = *Heaps().begin()->second.second;
  EXPECT_EQ(slab1->StartId(), PageId::FromPtr(slab_heap.Start()));
  EXPECT_EQ(slab2->StartId(), PageId::FromPtr(slab_heap.Start()) + 4);
  EXPECT_EQ(slab3->StartId(), PageId::FromPtr(slab_heap.Start()) + 2);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, AlignedHighlyAlignedSlab) {
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab1, Fixture().AllocateSlab(3));
  ASSERT_THAT(Fixture().AllocateSlab(32, 64 * kPageSize).status(), IsOk());
  ASSERT_OK_AND_DEFINE(AllocatedSlab*, slab3, Fixture().AllocateSlab(61));
  ASSERT_THAT(HeapsVec(), ElementsAre(_, _));

  EXPECT_EQ(slab1->StartId() + 3, slab3->StartId());
  EXPECT_THAT(ValidateHeap(), IsOk());
}

}  // namespace ckmalloc

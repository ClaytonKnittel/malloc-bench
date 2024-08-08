#include "src/ckmalloc/slab_manager.h"

#include <cstddef>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "gtest/gtest.h"
#include "util/gtest_util.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/testlib.h"

namespace ckmalloc {

using util::IsOk;

class SlabManagerTest : public CkMallocTest {
 public:
  SlabManagerTest() : heap_(64), slab_manager_(&heap_, &slab_map_) {}

  TestHeap& Heap() {
    return heap_;
  }

  TestSlabMap& SlabMap() {
    return slab_map_;
  }

  TestSlabManager& SlabManager() {
    return slab_manager_;
  }

  absl::Status ValidateHeap();

  absl::StatusOr<Slab*> AllocateSlab(uint32_t n_pages) {
    auto result = SlabManager().Alloc(1);
    if (!result.has_value()) {
      return nullptr;
    }
    auto [start_id, slab] = std::move(result.value());
    // Arbitrarily make all allocated slabs metadata slabs. Their actual type
    // doesn't matter, `SlabManager` only cares about free vs. not free.
    slab->InitMetadataSlab(start_id, n_pages);
    RETURN_IF_ERROR(slab_map_.AllocatePath(start_id, start_id + n_pages - 1));
    slab_map_.InsertRange(start_id, start_id + n_pages - 1, slab);
    return slab;
  }

 private:
  TestHeap heap_;
  TestSlabMap slab_map_;
  TestSlabManager slab_manager_;
};

absl::Status SlabManagerTest::ValidateHeap() {
  if (Heap().Size() % kPageSize != 0) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Expected heap size to be a multiple of page size, but was %zu",
        Heap().Size()));
  }

  PageId page = PageId::Zero();
  PageId end = page + Heap().Size() / kPageSize;
  Slab* previous_slab = nullptr;
  bool previous_was_free = false;
  while (page < end) {
    Slab* slab = SlabMap().FindSlab(page);
    if (slab == nullptr) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Unexpected `nullptr` slab map entry after end of "
                          "previous slab, at page %v",
                          page));
    }
    if (page != slab->StartId()) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Expected start of slab at page %v, found slab %v", page, *slab));
    }
    if (slab->Pages() > static_cast<uint32_t>(end - page)) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Slab %v extends beyond the end of the heap, which is %v pages",
          *slab, end));
    }
    switch (slab->Type()) {
      case SlabType::kUnmapped: {
        return absl::FailedPreconditionError(absl::StrFormat(
            "Unexpected unmapped slab found in slab map at page id %v", page));
      }
      case SlabType::kFree: {
        if (previous_was_free) {
          return absl::FailedPreconditionError(
              absl::StrFormat("Unexpected two adjacent free slabs: %v and %v",
                              *previous_slab, *slab));
        }
        previous_was_free = true;
        break;
      }
      case SlabType::kMetadata: {
        previous_was_free = false;
        break;
      }
      case SlabType::kSmall:
      case SlabType::kLarge: {
        return absl::FailedPreconditionError(absl::StrFormat(
            "Unexpected slab type (only metadata/free should exist): %v",
            *slab));
      }
    }

    page += slab->Pages();
    previous_slab = slab;
  }

  return absl::OkStatus();
}

TEST_F(SlabManagerTest, HeapStartIsPageIdZero) {
  SlabManager().Alloc(1);
  EXPECT_EQ(SlabManager().PageIdFromPtr(Heap().Start()), PageId(0));
}

TEST_F(SlabManagerTest, AllPtrsInFirstPageIdZero) {
  SlabManager().Alloc(1);
  for (size_t offset = 0; offset < kPageSize; offset++) {
    EXPECT_EQ(SlabManager().PageIdFromPtr(
                  static_cast<uint8_t*>(Heap().Start()) + offset),
              PageId(0));
  }
}

TEST_F(SlabManagerTest, PageIdIncreasesPerPage) {
  constexpr size_t kPages = 16;
  SlabManager().Alloc(kPages);
  for (size_t page_n = 0; page_n < kPages; page_n++) {
    void* beginning =
        static_cast<uint8_t*>(Heap().Start()) + page_n * kPageSize;
    void* end = static_cast<uint8_t*>(beginning) + kPageSize - 1;
    EXPECT_EQ(SlabManager().PageIdFromPtr(beginning), PageId(page_n));
    EXPECT_EQ(SlabManager().PageIdFromPtr(end), PageId(page_n));
  }
}

TEST_F(SlabManagerTest, SlabStartFromId) {
  constexpr size_t kPages = 16;
  SlabManager().Alloc(kPages);
  for (size_t page_n = 0; page_n < kPages; page_n++) {
    EXPECT_EQ(SlabManager().SlabStartFromId(PageId(page_n)),
              static_cast<uint8_t*>(Heap().Start()) + page_n * kPageSize);
  }
}

TEST_F(SlabManagerTest, EmptyHeapValid) {
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, SinglePageHeapValid) {
  ASSERT_OK_AND_DEFINE(Slab*, slab, AllocateSlab(1));
  EXPECT_EQ(slab->StartId(), PageId::Zero());
  EXPECT_THAT(ValidateHeap(), IsOk());
}

}  // namespace ckmalloc

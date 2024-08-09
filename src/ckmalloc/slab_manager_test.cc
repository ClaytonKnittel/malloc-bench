#include <cstddef>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "gtest/gtest.h"
#include "util/gtest_util.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/free_slab.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/testlib.h"

namespace ckmalloc {

using util::IsOk;

class SlabManagerTest : public CkMallocTest {
 public:
  static constexpr size_t kNumPages = 64;

  SlabManagerTest() : heap_(kNumPages), slab_manager_(&heap_, &slab_map_) {}

  TestHeap& Heap() {
    return heap_;
  }

  TestSlabMap& SlabMap() {
    return slab_map_;
  }

  TestSlabManager& SlabManager() {
    return slab_manager_;
  }

  PageId HeapEnd() const {
    return PageId(heap_.Size() / kPageSize);
  }

  absl::Status ValidateHeap();

  absl::StatusOr<Slab*> AllocateSlab(uint32_t n_pages) {
    auto result = slab_manager_.Alloc(n_pages);
    if (!result.has_value()) {
      return nullptr;
    }
    auto [start_id, slab] = std::move(result.value());
    PageId end_id = start_id + n_pages - 1;

    if (end_id >= HeapEnd()) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Allocated slab past end of heap: %v - %v extends beyond page %v",
          start_id, end_id, HeapEnd() - 1));
    }
    for (const auto& [slab, _] : allocated_slabs_) {
      if (start_id <= slab->EndId() && end_id >= slab->StartId()) {
        return absl::FailedPreconditionError(absl::StrFormat(
            "Allocated slab from page %v to %v, which overlaps with %v",
            start_id, end_id, *slab));
      }
    }

    // Arbitrarily make all allocated slabs metadata slabs. Their actual type
    // doesn't matter, `SlabManager` only cares about free vs. not free.
    slab->InitMetadataSlab(start_id, n_pages);
    RETURN_IF_ERROR(slab_map_.AllocatePath(start_id, end_id));
    slab_map_.InsertRange(start_id, end_id, slab);

    Slab copy;
    copy.InitMetadataSlab(start_id, n_pages);
    auto [it, inserted] = allocated_slabs_.insert({ slab, copy });
    if (!inserted) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Unexpected double-alloc of slab metadata %p for %v", slab, *slab));
    }

    return slab;
  }

  absl::Status FreeSlab(Slab* slab) {
    auto it = allocated_slabs_.find(slab);
    if (it == allocated_slabs_.end()) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Unexpected free of unallocated slab %v", *slab));
    }
    allocated_slabs_.erase(it);
    slab_manager_.Free(slab);
    return absl::OkStatus();
  }

 private:
  TestHeap heap_;
  TestSlabMap slab_map_;
  TestSlabManager slab_manager_;

  // Maps allocated slabs to a copy of their metadata.
  absl::flat_hash_map<Slab*, Slab> allocated_slabs_;
};

absl::Status SlabManagerTest::ValidateHeap() {
  if (Heap().Size() % kPageSize != 0) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Expected heap size to be a multiple of page size, but was %zu",
        Heap().Size()));
  }

  absl::flat_hash_set<Slab*> visited_slabs;
  PageId page = PageId::Zero();
  PageId end = HeapEnd();
  Slab* previous_slab = nullptr;
  bool previous_was_free = false;
  uint32_t free_slabs = 0;
  uint32_t allocated_slabs = 0;
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
          "Expected start of slab at page %v, found %v", page, *slab));
    }
    if (slab->Pages() > static_cast<uint32_t>(end - page)) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "%v extends beyond the end of the heap, which is %v pages", *slab,
          end));
    }

    if (visited_slabs.contains(slab)) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Found double occurrence of slab %v in the heap", *slab));
    }
    visited_slabs.insert(slab);

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
        free_slabs++;
        break;
      }
      case SlabType::kMetadata: {
        auto it = allocated_slabs_.find(slab);
        if (it == allocated_slabs_.end()) {
          return absl::FailedPreconditionError(
              absl::StrFormat("Encountered unallocated slab: %v", *slab));
        }
        if (slab->Type() != it->second.Type() ||
            slab->StartId() != it->second.StartId() ||
            slab->Pages() != it->second.Pages()) {
          return absl::FailedPreconditionError(absl::StrFormat(
              "Allocated slab metadata was dirtied: found %v, expected %v",
              *slab, it->second));
        }

        previous_was_free = false;
        allocated_slabs++;
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

  if (allocated_slabs != allocated_slabs_.size()) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Encountered %" PRIu32
        " allocated slabs when iterating over the heap, but expected %zu",
        allocated_slabs, allocated_slabs_.size()));
  }

  // Validate the single-page freelist.
  absl::flat_hash_set<Slab*> single_page_slabs;
  for (const FreeSinglePageSlab& slab_start :
       slab_manager_.single_page_freelist_) {
    PageId start_id = slab_manager_.PageIdFromPtr(&slab_start);
    Slab* slab = SlabMap().FindSlab(start_id);
    if (slab == nullptr) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Unexpected `nullptr` slab map entry in single-page "
                          "freelist, at page %v",
                          page));
    }
    if (slab->StartId() != start_id) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Unexpected non-slab-start in single-page freelist: "
                          "freelist entry on page %v, maps to %v",
                          start_id, *slab));
    }
    if (slab->Pages() != 1) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Unexpected multi-page slab in single-page slab freelist: %v",
          *slab));
    }
    if (!visited_slabs.contains(slab)) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Found slab not encountered when iterating over the "
                          "heap in single-page freelist: %v",
                          *slab));
    }
    if (single_page_slabs.contains(slab)) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Detected cycle in single-page freelist at %v", *slab));
    }
    single_page_slabs.insert(slab);
  }

  // Validate the multi-page freelist.
  absl::flat_hash_set<Slab*> multi_page_slabs;
  if (slab_manager_.smallest_multi_page_ != nullptr &&
      multi_page_slabs.empty()) {
    return absl::FailedPreconditionError(
        "Unexpected non-null smallest multi-page cache while "
        "multi-page slabs tree is empty");
  }
  for (const FreeMultiPageSlab& slab_start :
       slab_manager_.multi_page_free_slabs_) {
    PageId start_id = slab_manager_.PageIdFromPtr(&slab_start);
    Slab* slab = SlabMap().FindSlab(start_id);
    if (slab == nullptr) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Unexpected `nullptr` slab map entry in multi-page "
                          "free-tree, at page %v",
                          page));
    }
    if (slab->StartId() != start_id) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Unexpected non-slab-start in multi-page free-tree: "
                          "free-tree entry on page %v, maps to %v",
                          start_id, *slab));
    }
    if (slab->Pages() <= 1) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Unexpected single-page slab in multi-page slab free-tree: %v",
          *slab));
    }
    if (!visited_slabs.contains(slab)) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Found slab not encountered when iterating over the "
                          "heap in multi-page free-tree: %v",
                          *slab));
    }

    if (multi_page_slabs.empty()) {
      if (slab_manager_.smallest_multi_page_ != &slab_start) {
        return absl::FailedPreconditionError(absl::StrFormat(
            "smallest multi-page cache does not equal the first slab in the "
            "multi-page slab tree: %p (cache) vs. %v",
            slab_manager_.smallest_multi_page_, *slab));
      }
    }

    if (multi_page_slabs.contains(slab)) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Detected cycle in multi-page free-tree at %v", *slab));
    }
    multi_page_slabs.insert(slab);
  }

  if (single_page_slabs.size() + multi_page_slabs.size() != free_slabs) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Free single-page slabs + free multi-page slabs != free slabs "
        "encountered when iterating over the heap: %zu + %zu != %" PRIu32 "",
        single_page_slabs.size(), multi_page_slabs.size(), free_slabs));
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

TEST_F(SlabManagerTest, TwoAdjacentAllocatedSlabs) {
  ASSERT_OK_AND_DEFINE(Slab*, slab1, AllocateSlab(1));
  ASSERT_OK_AND_DEFINE(Slab*, slab2, AllocateSlab(1));
  EXPECT_EQ(slab1->StartId(), PageId::Zero());
  EXPECT_EQ(slab2->StartId(), PageId(1));
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, SingleLargeSlab) {
  ASSERT_OK_AND_DEFINE(Slab*, slab, AllocateSlab(9));
  EXPECT_EQ(slab->StartId(), PageId::Zero());
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, SlabTooLargeDoesNotAllocate) {
  ASSERT_OK_AND_DEFINE(Slab*, slab, AllocateSlab(kNumPages + 1));
  EXPECT_EQ(slab, nullptr);
  EXPECT_EQ(Heap().Size(), 0);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, FreeOnce) {
  ASSERT_OK_AND_DEFINE(Slab*, slab, AllocateSlab(1));
  ASSERT_THAT(FreeSlab(slab), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, FreeLarge) {
  ASSERT_OK_AND_DEFINE(Slab*, slab, AllocateSlab(12));
  ASSERT_THAT(FreeSlab(slab), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(SlabManagerTest, FreeTwice) {
  ASSERT_OK_AND_DEFINE(Slab*, slab1, AllocateSlab(1));
  ASSERT_OK_AND_DEFINE(Slab*, slab2, AllocateSlab(1));
  ASSERT_THAT(FreeSlab(slab1), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
  ASSERT_THAT(FreeSlab(slab2), IsOk());
  EXPECT_THAT(ValidateHeap(), IsOk());
}

}  // namespace ckmalloc

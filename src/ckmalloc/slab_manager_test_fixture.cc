#include "src/ckmalloc/slab_manager_test_fixture.h"

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "util/absl_util.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/testlib.h"
#include "src/ckmalloc/util.h"
#include "src/rng.h"

namespace ckmalloc {

using TestSlabManager = SlabManagerFixture::TestSlabManager;

TestSlabManager::TestSlabManager(SlabManagerFixture* test_fixture,
                                 TestHeap* heap, TestSlabMap* slab_map)
    : test_fixture_(test_fixture), slab_manager_(heap, slab_map) {}

void* TestSlabManager::PageStartFromId(PageId page_id) const {
  return slab_manager_.PageStartFromId(page_id);
}

PageId TestSlabManager::PageIdFromPtr(const void* ptr) const {
  return slab_manager_.PageIdFromPtr(ptr);
}

std::optional<std::pair<PageId, Slab*>> TestSlabManager::Alloc(
    uint32_t n_pages) {
  using AllocResult = std::pair<PageId, Slab*>;
  DEFINE_OR_RETURN_OPT(AllocResult, result, slab_manager_.Alloc(n_pages));
  return result;
}

void TestSlabManager::Free(AllocatedSlab* slab) {
  auto it = test_fixture_->allocated_slabs_.find(slab);
  CK_ASSERT_FALSE(it == test_fixture_->allocated_slabs_.end());

  test_fixture_->allocated_slabs_.erase(it);
  slab_manager_.Free(slab);
}

Block* TestSlabManager::FirstBlockInLargeSlab(LargeSlab* slab) {
  return slab_manager_.FirstBlockInLargeSlab(slab);
}

void TestSlabManager::HandleAlloc(AllocatedSlab* slab) {
  // Make a copy of this slab's metadata to ensure it does not get dirtied.
  AllocatedSlab copy = *slab;
  auto [it, inserted] = test_fixture_->allocated_slabs_.insert({ slab, copy });
  CK_ASSERT_TRUE(inserted);
}

absl::Status SlabManagerFixture::ValidateHeap() {
  if (Heap().Size() % kPageSize != 0) {
    return FailedTest(
        "Expected heap size to be a multiple of page size, but was %zu",
        Heap().Size());
  }

  absl::flat_hash_set<MappedSlab*> visited_slabs;
  PageId page = PageId::Zero();
  PageId end = HeapEndId();
  MappedSlab* previous_slab = nullptr;
  bool previous_was_free = false;
  uint32_t free_slabs = 0;
  uint32_t allocated_slabs = 0;
  while (page < end) {
    MappedSlab* slab = SlabMap().FindSlab(page);
    if (slab == nullptr) {
      // This must be a metadata slab.
      page += 1;
      continue;
    }
    if (page != slab->StartId()) {
      return FailedTest(
          "Slab metadata incorrect, start of slab should be page %v, found %v",
          page, *slab);
    }
    if (slab->Pages() > static_cast<uint32_t>(end - page)) {
      return FailedTest(
          "%v extends beyond the end of the heap, which is %v pages", *slab,
          end);
    }

    if (visited_slabs.contains(slab)) {
      return FailedTest("Found double occurrence of slab %v in the heap",
                        *slab);
    }
    visited_slabs.insert(slab);

    switch (slab->Type()) {
      case SlabType::kUnmapped: {
        return FailedTest(
            "Unexpected unmapped slab found in slab map at page id %v", page);
      }
      case SlabType::kFree: {
        if (previous_was_free) {
          return FailedTest("Unexpected two adjacent free slabs: %v and %v",
                            *previous_slab, *slab);
        }
        MappedSlab* first_page_slab = SlabMap().FindSlab(slab->StartId());
        MappedSlab* last_page_slab = SlabMap().FindSlab(slab->EndId());
        if (first_page_slab != slab || last_page_slab != slab) {
          return FailedTest(
              "Start and end pages of free slab do not map to the correct "
              "metadata: %v, start_id maps to %v, end_id maps to %v",
              *slab, first_page_slab, last_page_slab);
        }

        previous_was_free = true;
        free_slabs++;
        break;
      }
      case SlabType::kSmall:
      case SlabType::kLarge: {
        AllocatedSlab* allocated_slab = slab->ToAllocated();
        auto it = allocated_slabs_.find(allocated_slab);
        if (it == allocated_slabs_.end()) {
          return FailedTest("Encountered unallocated slab: %v", *slab);
        }

        const AllocatedSlab& slab_copy = it->second;

        if (allocated_slab->Type() != slab_copy.Type() ||
            allocated_slab->StartId() != slab_copy.StartId() ||
            allocated_slab->Pages() != slab_copy.Pages()) {
          return FailedTest(
              "Allocated slab metadata was dirtied: found %v, expected %v",
              *allocated_slab, slab_copy);
        }

        // Magic values are only used for allocations done through the slab
        // manager interface, since any other test which uses this fixture will
        // want to write to the allocated slabs.
        auto magic_it = slab_magics_.find(allocated_slab);
        if (magic_it != slab_magics_.end()) {
          uint64_t magic = magic_it->second;
          RETURN_IF_ERROR(CheckMagic(allocated_slab, magic));
        }

        for (PageId page_id = allocated_slab->StartId();
             page_id <= allocated_slab->EndId(); ++page_id) {
          MappedSlab* mapped_slab = SlabMap().FindSlab(page_id);
          if (mapped_slab != allocated_slab) {
            return FailedTest(
                "Internal page %v of %v does not map to the correct slab "
                "metadata: %v",
                page_id, *allocated_slab, mapped_slab);
          }
        }

        previous_was_free = false;
        allocated_slabs++;
        break;
      }
    }

    page += slab->Pages();
    previous_slab = slab;
  }

  if (allocated_slabs != allocated_slabs_.size()) {
    return FailedTest(
        "Encountered %" PRIu32
        " allocated slabs when iterating over the heap, but expected %zu",
        allocated_slabs, allocated_slabs_.size());
  }

  // Validate the single-page freelist.
  absl::flat_hash_set<class FreeSlab*> single_page_slabs;
  for (const FreeSinglePageSlab& slab_start :
       SlabManager().Underlying().single_page_freelist_) {
    PageId start_id = SlabManager().PageIdFromPtr(&slab_start);
    MappedSlab* slab = SlabMap().FindSlab(start_id);
    if (slab == nullptr) {
      return FailedTest(
          "Unexpected `nullptr` slab map entry in single-page freelist, at "
          "page %v",
          page);
    }
    if (slab->Type() != SlabType::kFree) {
      return FailedTest(
          "Unexpeted non-free slab in single-page slab freelist: %v", *slab);
    }
    if (slab->StartId() != start_id) {
      return FailedTest(
          "Unexpected non-slab-start in single-page freelist: freelist entry "
          "on page %v, maps to %v",
          start_id, *slab);
    }
    if (slab->Pages() != 1) {
      return FailedTest(
          "Unexpected multi-page slab in single-page slab freelist: %v", *slab);
    }
    if (!visited_slabs.contains(slab)) {
      return FailedTest(
          "Found slab not encountered when iterating over the heap in "
          "single-page freelist: %v",
          *slab);
    }
    if (single_page_slabs.contains(slab->ToFree())) {
      return FailedTest("Detected cycle in single-page freelist at %v", *slab);
    }
    single_page_slabs.insert(slab->ToFree());
  }

  // Validate the multi-page freelist.
  absl::flat_hash_set<class FreeSlab*> multi_page_slabs;
  if (SlabManager().Underlying().smallest_multi_page_ != nullptr &&
      SlabManager().Underlying().multi_page_free_slabs_.Empty()) {
    return FailedTest(
        "Unexpected non-null smallest multi-page cache while "
        "multi-page slabs tree is empty: %p",
        SlabManager().Underlying().smallest_multi_page_);
  }
  for (const FreeMultiPageSlab& slab_start :
       SlabManager().Underlying().multi_page_free_slabs_) {
    PageId start_id = SlabManager().PageIdFromPtr(&slab_start);
    MappedSlab* slab = SlabMap().FindSlab(start_id);
    if (slab == nullptr) {
      return FailedTest(
          "Unexpected `nullptr` slab map entry in multi-page free-tree, at "
          "page %v",
          page);
    }
    if (slab->Type() != SlabType::kFree) {
      return FailedTest(
          "Unexpeted non-free slab in single-page slab freelist: %v", *slab);
    }
    if (slab->StartId() != start_id) {
      return FailedTest(
          "Unexpected non-slab-start in multi-page free-tree: free-tree entry "
          "on page %v, maps to %v",
          start_id, *slab);
    }
    if (slab->Pages() <= 1) {
      return FailedTest(
          "Unexpected single-page slab in multi-page slab free-tree: %v",
          *slab);
    }
    if (!visited_slabs.contains(slab)) {
      return FailedTest(
          "Found slab not encountered when iterating over the heap in "
          "multi-page free-tree: %v",
          *slab);
    }

    if (multi_page_slabs.empty()) {
      if (SlabManager().Underlying().smallest_multi_page_ != &slab_start) {
        return FailedTest(
            "smallest multi-page cache does not equal the first slab in the "
            "multi-page slab tree: %p (cache) vs. %v",
            SlabManager().Underlying().smallest_multi_page_, *slab);
      }
    }

    if (multi_page_slabs.contains(slab->ToFree())) {
      return FailedTest("Detected cycle in multi-page free-tree at %v", *slab);
    }
    multi_page_slabs.insert(slab->ToFree());
  }

  if (single_page_slabs.size() + multi_page_slabs.size() != free_slabs) {
    return FailedTest(
        "Free single-page slabs + free multi-page slabs != free slabs "
        "encountered when iterating over the heap: %zu + %zu != %" PRIu32 "",
        single_page_slabs.size(), multi_page_slabs.size(), free_slabs);
  }

  return absl::OkStatus();
}

absl::Status SlabManagerFixture::ValidateEmpty() {
  PageId page = PageId::Zero();
  PageId end = HeapEndId();
  while (page < end) {
    MappedSlab* slab = SlabMap().FindSlab(page);
    if (slab == nullptr) {
      // This must be a metadata slab.
      page += 1;
      continue;
    }

    if (slab->Type() != SlabType::kFree) {
      return FailedTest("Unexpected non-free slab found in heap: %v.", *slab);
    }

    page += slab->Pages();
  }

  return absl::OkStatus();
}

absl::StatusOr<AllocatedSlab*> SlabManagerFixture::AllocateSlab(
    uint32_t n_pages) {
  // Arbitrarily make all allocated slabs large slabs. Their actual type doesn't
  // matter, `SlabManager` only cares about free vs. not free.
  auto result = SlabManager().template Alloc<LargeSlab>(n_pages);
  if (!result.has_value()) {
    return nullptr;
  }
  auto [start_id, slab] = std::move(result.value());
  PageId end_id = start_id + n_pages - 1;

  if (end_id >= HeapEndId()) {
    return FailedTest(
        "Allocated slab past end of heap: %v - %v extends beyond page %v",
        start_id, end_id, HeapEndId() - 1);
  }
  for (const auto& [other_slab, _] : allocated_slabs_) {
    // Don't check for collision with ourselves.
    if (slab == other_slab) {
      continue;
    }

    if (slab->StartId() <= other_slab->EndId() &&
        slab->EndId() >= other_slab->StartId()) {
      return FailedTest("Allocated slab %v, which overlaps with %v", *slab,
                        *other_slab);
    }
  }

  uint64_t magic = rng_.GenRand64();
  FillMagic(slab, magic);

  // Make a copy of this slab's metadata to ensure it does not get dirtied.
  auto [it, inserted] = slab_magics_.insert({ slab, magic });
  CK_ASSERT_TRUE(inserted);

  return slab;
}

absl::Status SlabManagerFixture::FreeSlab(AllocatedSlab* slab) {
  auto it = slab_magics_.find(slab);
  if (it == slab_magics_.end()) {
    return FailedTest("Unexpected free of unallocated slab %v", *slab);
  }

  RETURN_IF_ERROR(CheckMagic(slab, it->second));

  slab_magics_.erase(it);
  SlabManager().Free(slab);
  return absl::OkStatus();
}

void SlabManagerFixture::FillMagic(AllocatedSlab* slab, uint64_t magic) {
  CK_ASSERT_EQ(slab->Type(), SlabType::kLarge);
  auto* start = reinterpret_cast<uint64_t*>(
      SlabManager().PageStartFromId(slab->StartId()));
  auto* end = reinterpret_cast<uint64_t*>(
      static_cast<uint8_t*>(SlabManager().PageStartFromId(slab->EndId())) +
      kPageSize);

  for (; start != end; start++) {
    *start = magic;
  }
}

absl::Status SlabManagerFixture::CheckMagic(AllocatedSlab* slab,
                                            uint64_t magic) {
  CK_ASSERT_EQ(slab->Type(), SlabType::kLarge);
  auto* start = reinterpret_cast<uint64_t*>(
      SlabManager().PageStartFromId(slab->StartId()));
  auto* end = reinterpret_cast<uint64_t*>(
      static_cast<uint8_t*>(SlabManager().PageStartFromId(slab->EndId())) +
      kPageSize);

  for (; start != end; start++) {
    if (*start != magic) {
      auto* begin = reinterpret_cast<uint64_t*>(
          SlabManager().PageStartFromId(slab->StartId()));
      return FailedTest(
          "Allocated metadata slab %v was dirtied starting from offset %zu",
          *slab, (start - begin) * sizeof(uint64_t));
    }
  }

  return absl::OkStatus();
}

}  // namespace ckmalloc

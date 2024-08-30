#pragma once

#include <string>

#include "absl/strings/str_format.h"

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/common.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/util.h"
#include "src/heap_interface.h"

namespace ckmalloc {

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
class HeapPrinter {
 public:
  HeapPrinter(const bench::Heap* heap, const SlabMap* slab_map,
              const SlabManager* slab_manager);

  std::string Print();

 private:
  static constexpr size_t kMaxRowLength = kPageSize / kDefaultAlignment / 2;

  static std::string PrintMetadata(PageId page_id);

  static std::string PrintFree(const FreeSlab* slab);

  std::string PrintSmall(const SmallSlab* slab);

  std::string PrintBlocked(const BlockedSlab* slab);

  std::string PrintSingleAlloc(const SingleAllocSlab* slab);

  const bench::Heap* const heap_;
  const SlabMap* const slab_map_;
  const SlabManager* const slab_manager_;
};

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
HeapPrinter<SlabMap, SlabManager>::HeapPrinter(const bench::Heap* heap,
                                               const SlabMap* slab_map,
                                               const SlabManager* slab_manager)
    : heap_(heap), slab_map_(slab_map), slab_manager_(slab_manager) {}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
std::string HeapPrinter<SlabMap, SlabManager>::Print() {
  std::string result;

  for (PageId page_id = PageId::Zero();
       page_id < PageId(heap_->Size() / kPageSize);) {
    MappedSlab* slab = slab_map_->FindSlab(page_id);
    if (slab == nullptr) {
      // Assume this is a metadata slab.
      result += PrintMetadata(page_id);
      result += "\n";
      ++page_id;
      continue;
    }

    switch (slab->Type()) {
      case SlabType::kUnmapped: {
        CK_ASSERT_TRUE(false);
      }
      case SlabType::kFree: {
        result += PrintFree(slab->ToFree());
        break;
      }
      case SlabType::kSmall: {
        result += PrintSmall(slab->ToSmall());
        break;
      }
      case SlabType::kBlocked: {
        result += PrintBlocked(slab->ToBlocked());
        break;
      }
      case SlabType::kSingleAlloc: {
        result += PrintSingleAlloc(slab->ToSingleAlloc());
        break;
      }
    }

    result += "\n";

    page_id += slab->Pages();
  }

  return result;
}

/* static */
template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
std::string HeapPrinter<SlabMap, SlabManager>::PrintMetadata(PageId page_id) {
  return absl::StrFormat("Page %v: metadata", page_id);
}

/* static */
template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
std::string HeapPrinter<SlabMap, SlabManager>::PrintFree(const FreeSlab* slab) {
  return absl::StrFormat("Pages %v - %v: free", slab->StartId(), slab->EndId());
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
std::string HeapPrinter<SlabMap, SlabManager>::PrintSmall(
    const SmallSlab* slab) {
  std::string result = absl::StrFormat(
      "Page %v: small %v %v%% full", slab->StartId(), slab->SizeClass(),
      100.F * slab->AllocatedSlices() /
          static_cast<float>(slab->SizeClass().MaxSlicesPerSlab()));

  // Track which slices in the small slab are free. Start off with all marked as
  // allocated, then go through and mark each free slab in the freelist as free.
  std::vector<bool> free_slots(slab->SizeClass().MaxSlicesPerSlab(), false);
  slab->IterateSlices(
      slab_manager_->PageStartFromId(slab->StartId()),
      [&free_slots](uint32_t slice_idx) { free_slots[slice_idx] = true; });

  result += "\n[";
  uint32_t offset = 1;
  for (bool free_slot : free_slots) {
    if (offset == kMaxRowLength) {
      result += "\n";
      offset = 0;
    }

    result += free_slot ? '.' : 'X';
  }
  result += ']';

  return result;
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
std::string HeapPrinter<SlabMap, SlabManager>::PrintBlocked(
    const BlockedSlab* slab) {
  std::string result = absl::StrFormat(
      "Pages %v - %v: large %v%% full", slab->StartId(), slab->EndId(),
      100.F * slab->AllocatedBytes() /
          static_cast<float>(slab->Pages() * kPageSize));

  std::vector<std::string> rows(2 * slab->Pages(),
                                std::string(kMaxRowLength, '.'));

  auto set_row = [&rows](uint64_t i, char c) {
    rows[i / kMaxRowLength][i % kMaxRowLength] = c;
  };

  uint64_t offset = 1;
  for (Block* block = slab_manager_->FirstBlockInBlockedSlab(slab);
       block->Size() != 0; block = block->NextAdjacentBlock()) {
    uint64_t block_size = block->Size() / kDefaultAlignment;

    if (!block->Free()) {
      set_row(offset, '[');
      for (uint64_t i = offset + 1; i < offset + block_size - 1; i++) {
        set_row(i, '=');
      }

      set_row(offset + block_size - 1, ']');
    }

    offset += block_size;
  }

  for (const auto& row : rows) {
    result += "\n" + row;
  }

  return result;
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager>
std::string HeapPrinter<SlabMap, SlabManager>::PrintSingleAlloc(
    const SingleAllocSlab* slab) {
  std::string result = absl::StrFormat("Pages %v - %v: single-alloc",
                                       slab->StartId(), slab->EndId());
  return result;
}

}  // namespace ckmalloc

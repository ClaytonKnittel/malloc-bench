#include "src/ckmalloc/heap_printer.h"

#include <string>

#include "absl/strings/str_format.h"

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/common.h"
#include "src/ckmalloc/metadata_manager.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/size_class.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_manager.h"
#include "src/ckmalloc/slab_map.h"
#include "src/ckmalloc/util.h"
#include "src/heap_interface.h"

namespace ckmalloc {

HeapPrinter::HeapPrinter(const bench::Heap* heap, const SlabMap* slab_map,
                         const SlabManager* slab_manager,
                         const MetadataManager* metadata_manager)
    : heap_(heap),
      slab_map_(slab_map),
      slab_manager_(slab_manager),
      metadata_manager_(metadata_manager) {}

std::string HeapPrinter::Print() {
  std::string result;

  if (heap_ == metadata_manager_->MetadataHeap()) {
    for (PageId page_id = PageId::Zero();
         page_id < PageId(heap_->Size() / kPageSize); ++page_id) {
      result += absl::StrFormat("Metadata slab: %v\n", page_id);
    }
    return result;
  }

  for (PageId page_id = PageId::Zero();
       page_id < PageId(heap_->Size() / kPageSize);) {
    MappedSlab* slab = slab_map_->FindSlab(page_id);
    CK_ASSERT_NE(slab, nullptr);

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
std::string HeapPrinter::PrintMetadata(PageId page_id) {
  return absl::StrFormat("Page %v: metadata", page_id);
}

/* static */
std::string HeapPrinter::PrintFree(const FreeSlab* slab) {
  return absl::StrFormat("Pages %v%v: free", slab->StartId(),
                         slab->StartId() == slab->EndId()
                             ? ""
                             : absl::StrFormat(" - %v", slab->EndId()));
}

std::string HeapPrinter::PrintSmall(const SmallSlab* slab) {
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

  result += "\n";
  uint32_t offset = 0;
  if (slab->SizeClass().SliceSize() == kMinAlignment) {
    for (size_t i = 0; i < free_slots.size(); i += 2) {
      if (offset == kMaxRowLength) {
        result += "\n";
        offset = 0;
      }

      if (free_slots[i] && free_slots[i + 1]) {
        result += ' ';
      } else if (free_slots[i] && !free_slots[i + 1]) {
        result += ',';
      } else if (!free_slots[i] && free_slots[i + 1]) {
        result += '`';
      } else {
        result += '\\';
      }

      offset++;
    }
  } else {
    uint32_t width = slab->SizeClass().SliceSize() / kDefaultAlignment;
    for (bool free_slot : free_slots) {
      for (size_t w = 0; w < width; w++) {
        if (offset == kMaxRowLength) {
          result += "\n";
          offset = 0;
        }

        result +=
            free_slot ? '.' : (w == 0 ? '[' : (w == width - 1 ? ']' : 'X'));
        offset++;
      }
    }
  }

  return result;
}

std::string HeapPrinter::PrintBlocked(const BlockedSlab* slab) {
  std::string result =
      absl::StrFormat("Pages %v%v: large %v%% full", slab->StartId(),
                      slab->StartId() == slab->EndId()
                          ? ""
                          : absl::StrFormat(" - %v", slab->EndId()),
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

std::string HeapPrinter::PrintSingleAlloc(const SingleAllocSlab* slab) {
  std::string result =
      absl::StrFormat("Pages %v%v: single-alloc", slab->StartId(),
                      slab->StartId() == slab->EndId()
                          ? ""
                          : absl::StrFormat(" - %v", slab->EndId()));

  for (const auto& row :
       std::vector(2 * slab->Pages(), std::string(kMaxRowLength, '='))) {
    result += '\n' + row;
  }

  return result;
}

}  // namespace ckmalloc

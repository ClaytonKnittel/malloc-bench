#include "src/ckmalloc/heap_printer.h"

#include <cstdint>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "util/print_colors.h"

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/common.h"
#include "src/ckmalloc/heap_iterator.h"
#include "src/ckmalloc/metadata_manager.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/size_class.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_manager.h"
#include "src/ckmalloc/slab_map.h"
#include "src/ckmalloc/slice_id.h"
#include "src/ckmalloc/util.h"
#include "src/heap_interface.h"

namespace ckmalloc {

HeapPrinter::HeapPrinter(const bench::Heap* heap, SlabMap* slab_map,
                         const SlabManager* slab_manager,
                         const MetadataManager* metadata_manager)
    : heap_(heap),
      slab_map_(slab_map),
      slab_manager_(slab_manager),
      metadata_manager_(metadata_manager) {}

HeapPrinter& HeapPrinter::WithHighlightAddr(void* addr, const char* color_fmt) {
  highlight_addrs_.insert_or_assign(addr, color_fmt);
  return *this;
}

std::string HeapPrinter::Print() {
  std::string result;

  if (heap_ == metadata_manager_->heap_) {
    // TODO: print the metadata.
    result += absl::StrFormat("Metadata size: %zu bytes (%zu pages)",
                              heap_->Size(), CeilDiv(heap_->Size(), kPageSize));
    return result;
  }

  for (auto slab_it = HeapIterator::HeapBegin(heap_, slab_map_);
       slab_it != HeapIterator(); ++slab_it) {
    MappedSlab* slab = *slab_it;
    CK_ASSERT_NE(slab, nullptr);

    switch (slab->Type()) {
      case SlabType::kUnmapped: {
        CK_UNREACHABLE("Unexpected unmapped slab");
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
      case SlabType::kMmap: {
        result += PrintMmap(slab->ToMmap());
        break;
      }
    }

    result += "\n";
  }

  return result;
}

/* static */
std::string HeapPrinter::PrintMetadata(PageId page_id) {
  return absl::StrFormat("Page %v: metadata", page_id);
}

std::string HeapPrinter::PrintFree(const FreeSlab* slab) {
  std::string result = absl::StrFormat(
      "Pages %v%v: free", slab->StartId() - HeapStartId(),
      slab->StartId() == slab->EndId()
          ? ""
          : absl::StrFormat(" - %v", slab->EndId() - HeapStartId()));

  std::vector<std::string> rows(2 * slab->Pages(),
                                std::string(kMaxRowLength, '.'));
  for (const auto& row : rows) {
    result += "\n" + row;
  }

  return result;
}

std::string HeapPrinter::PrintSmall(const SmallSlab* slab) {
  std::string result = absl::StrFormat(
      "Page %v: small %v %v%% full", slab->StartId() - HeapStartId(),
      slab->SizeClass(),
      100.F * slab->AllocatedSlices() /
          static_cast<float>(slab->SizeClass().MaxSlicesPerSlab()));

  // Track which slices in the small slab are free. Start off with all marked as
  // allocated, then go through and mark each free slab in the freelist as free.
  std::vector<bool> free_slots(slab->SizeClass().MaxSlicesPerSlab(), false);
  void* const slab_start = slab->StartId().PageStart();
  slab->IterateSlices(slab_start, [&free_slots](uint32_t slice_idx) {
    free_slots[slice_idx] = true;
  });

  result += "\n";
  uint32_t offset = 0;
  if (slab->SizeClass().SliceSize() == kMinAlignment) {
    for (size_t i = 0; i < free_slots.size(); i += 2) {
      if (offset == kMaxRowLength) {
        result += "\n";
        offset = 0;
      }

      void* slice1 = slab->mapped.small.tiny_meta_
                         .SliceFromId(slab_start, TinySliceId::FromIdx(i))
                         ->ToAllocated()
                         ->UserDataPtr();
      void* slice2 = slab->mapped.small.tiny_meta_
                         .SliceFromId(slab_start, TinySliceId::FromIdx(i + 1))
                         ->ToAllocated()
                         ->UserDataPtr();

      auto it1 = highlight_addrs_.find(slice1);
      auto it2 = highlight_addrs_.find(slice2);
      if (it1 != highlight_addrs_.end()) {
        result += it1->second;
      } else if (it2 != highlight_addrs_.end()) {
        result += it2->second;
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

      if (it1 != highlight_addrs_.end() || it2 != highlight_addrs_.end()) {
        result += P_RESET;
      }

      offset++;
    }
  } else {
    uint32_t width = slab->SizeClass().SliceSize() / kDefaultAlignment;
    for (uint32_t i = 0; i < free_slots.size(); i++) {
      void* slice;
      if (slab->IsTiny()) {
        slice = slab->mapped.small.tiny_meta_
                    .SliceFromId(slab_start, TinySliceId::FromIdx(i))
                    ->ToAllocated()
                    ->UserDataPtr();
      } else {
        slice = slab->mapped.small.small_meta_
                    .SliceFromId(slab_start, SmallSliceId::FromIdx(i))
                    ->ToAllocated()
                    ->UserDataPtr();
      }

      auto it = highlight_addrs_.find(slice);
      const char* fmt_start = "";
      const char* fmt_end = "";
      if (it != highlight_addrs_.end()) {
        fmt_start = it->second;
        fmt_end = P_RESET;
      }

      result += fmt_start;
      for (size_t w = 0; w < width; w++) {
        if (offset == kMaxRowLength) {
          result += absl::StrCat(fmt_end, "\n", fmt_start);
          offset = 0;
        }

        result +=
            free_slots[i] ? '.' : (w == 0 ? '[' : (w == width - 1 ? ']' : 'X'));
        offset++;
      }
      result += fmt_end;
    }
  }

  return result;
}

std::string HeapPrinter::PrintBlocked(const BlockedSlab* slab) {
  std::string result = absl::StrFormat(
      "Pages %v%v: large %v%% full", slab->StartId() - HeapStartId(),
      slab->StartId() == slab->EndId()
          ? ""
          : absl::StrFormat(" - %v", slab->EndId() - HeapStartId()),
      100.F * slab->AllocatedBytes() /
          static_cast<float>(slab->Pages() * kPageSize));

  std::vector<std::string> rows(2 * slab->Pages());

  uint64_t offset = 0;
  auto push_entry_silent = [&rows, &offset](const char* str) {
    if (offset != rows.size() * kMaxRowLength) {
      rows[offset / kMaxRowLength] += str;
    }
  };
  auto push_entry = [&rows, &offset](char c, const char* fmt = "") {
    rows[offset / kMaxRowLength] += c;
    if ((offset + 1) % kMaxRowLength == 0) {
      rows[offset / kMaxRowLength] += P_RESET;
    }
    ++offset;
    if (offset < rows.size() * kMaxRowLength && offset % kMaxRowLength == 0) {
      rows[offset / kMaxRowLength] += fmt;
    }
  };

  push_entry('.');

  for (Block* block = slab_manager_->FirstBlockInBlockedSlab(slab);
       !block->IsPhonyHeader(); block = block->NextAdjacentBlock()) {
    uint64_t block_size = block->Size() / kDefaultAlignment;

    if (!block->Free()) {
      auto it = highlight_addrs_.find(block->ToAllocated()->UserDataPtr());
      const char* fmt = "";
      if (it != highlight_addrs_.end()) {
        fmt = it->second;
        push_entry_silent(it->second);
      }

      push_entry('[', fmt);
      for (uint64_t i = 0; i < block_size - 2; i++) {
        push_entry('=', fmt);
      }
      push_entry(']', fmt);

      if (it != highlight_addrs_.end()) {
        push_entry_silent(P_RESET);
      }
    } else {
      for (uint64_t i = 0; i < block_size; i++) {
        push_entry('.');
      }
    }
  }

  for (const auto& row : rows) {
    result += "\n" + row;
  }

  return result;
}

std::string HeapPrinter::PrintSingleAlloc(const SingleAllocSlab* slab) {
  std::string result = absl::StrFormat(
      "Pages %v%v: single-alloc", slab->StartId() - HeapStartId(),
      slab->StartId() == slab->EndId()
          ? ""
          : absl::StrFormat(" - %v", slab->EndId() - HeapStartId()));

  void* alloc = slab->StartId().PageStart();
  auto it = highlight_addrs_.find(alloc);

  for (const auto& row :
       std::vector(2 * slab->Pages(), std::string(kMaxRowLength, '='))) {
    result += '\n';
    if (it != highlight_addrs_.end()) {
      result += it->second;
    }
    result += row;
    if (it != highlight_addrs_.end()) {
      result += P_RESET;
    }
  }

  return result;
}

std::string HeapPrinter::PrintMmap(const MmapSlab* slab) {
  std::string result = absl::StrFormat(
      "Pages %v%v: mmapped", slab->StartId() - HeapStartId(),
      slab->StartId() == slab->EndId()
          ? ""
          : absl::StrFormat(" - %v", slab->EndId() - HeapStartId()));

  return result;
}

PageId HeapPrinter::HeapStartId() const {
  return PageId::FromPtr(heap_->Start());
}

}  // namespace ckmalloc

#include "src/ckmalloc/small_freelist_test_fixture.h"

#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"

#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/size_class.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_manager_test_fixture.h"
#include "src/ckmalloc/small_freelist.h"

namespace ckmalloc {

absl::Status SmallFreelistFixture::ValidateHeap() {
  // Iterate over the heap and collect all small slabs.
  std::array<absl::btree_set<SmallSlab*>, SizeClass::kNumSizeClasses> slabs;
  absl::btree_map<PageId, SmallSlab*> id_to_slab;
  for (auto it = slab_manager_test_fixture_->HeapBegin();
       it != slab_manager_test_fixture_->HeapEnd(); ++it) {
    if (*it == nullptr || it->Type() != SlabType::kSmall) {
      continue;
    }
    SmallSlab* slab = it->ToSmall();

    if (slab->Empty()) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Encountered empty slab at page %v in freelist", slab->StartId()));
    }

    if (!slab->Full()) {
      slabs[slab->SizeClass().Ordinal()].insert(slab);
    }
    id_to_slab[slab->StartId()] = slab;
  }

  // Verify that each freelist contains the expected elements.
  for (size_t i = 0; i < SizeClass::kNumSizeClasses; i++) {
    absl::btree_set<SmallSlab*> freelist_slabs;
    SizeClass size_class = SizeClass::FromOrdinal(i);

    PageId prev_id = PageId::Nil();
    for (PageId page_id =
             small_freelist_->FreelistHead(SizeClass::FromOrdinal(i));
         page_id != PageId::Nil();) {
      auto it = id_to_slab.find(page_id);
      if (it == id_to_slab.end()) {
        return absl::FailedPreconditionError(
            absl::StrFormat("Encountered slab in freelist %v which is not a "
                            "small slab at page id %v.",
                            size_class, page_id));
      }
      SmallSlab* slab = it->second;
      freelist_slabs.insert(slab);

      if (slab->Full()) {
        return absl::FailedPreconditionError(absl::StrFormat(
            "Encountered full slab at page %v in freelist", slab->StartId()));
      }

      if (slab->SizeClass() != size_class) {
        return absl::FailedPreconditionError(
            absl::StrFormat("Encountered slab of incorrect size class in "
                            "freelist: found %v, expected %v",
                            slab->SizeClass(), size_class));
      }

      if (prev_id != slab->PrevFree()) {
        return absl::FailedPreconditionError(
            absl::StrFormat("Prev ID of slab at page %v was %v, expected %v",
                            page_id, slab->PrevFree(), prev_id));
      }

      prev_id = page_id;
      page_id = slab->NextFree();
    }

    if (freelist_slabs != slabs[i]) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Freelist slabs for size class %v do not match "
                          "those found in the heap, some slabs are missing.",
                          size_class));
    }
  }

  return absl::OkStatus();
}

}  // namespace ckmalloc

#pragma once

#include <cstddef>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_manager.h"
#include "src/ckmalloc/testlib.h"
#include "src/rng.h"

namespace ckmalloc {

class SlabManagerTest : public CkMallocTest {
 public:
  static constexpr size_t kNumPages = 64;

  class TestSlabManager {
   public:
    using SlabManagerT = SlabManagerImpl<TestGlobalMetadataAlloc, TestSlabMap>;

    TestSlabManager(class SlabManagerTest* test_fixture, TestHeap* heap,
                    TestSlabMap* slab_map);

    SlabManagerT& UnderlyingMgr() {
      return slab_manager_;
    }

    void* PageStartFromId(PageId page_id) const;

    PageId PageIdFromPtr(const void* ptr) const;

    std::optional<SlabMgrAllocResult> Alloc(uint32_t n_pages,
                                            SlabType slab_type);

    void Free(Slab* slab);

   private:
    class SlabManagerTest* test_fixture_;
    SlabManagerT slab_manager_;
  };

  explicit SlabManagerTest(size_t n_pages)
      : heap_(n_pages),
        slab_manager_(this, &heap_, &slab_map_),
        rng_(1027, 3) {}

  SlabManagerTest() : SlabManagerTest(kNumPages) {}

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

  absl::Status ValidateHeap() override;

  absl::StatusOr<Slab*> AllocateSlab(uint32_t n_pages);

  absl::Status FreeSlab(Slab* slab);

 private:
  void FillMagic(Slab* slab, uint64_t magic);

  absl::Status CheckMagic(Slab* slab, uint64_t magic);

  TestHeap heap_;
  TestSlabMap slab_map_;
  TestSlabManager slab_manager_;
  util::Rng rng_;

  // Maps allocated slabs to a copy of their metadata.
  absl::flat_hash_map<Slab*, Slab> allocated_slabs_;

  // Maps allocated slabs to a copy of their magic number which is copied into
  // the whole slab. This is only used when slabs are allocated through the
  // `SlabManagerTestFixture` interface, since other tests that use this fixture
  // will write to the allocated slabs.
  absl::flat_hash_map<Slab*, uint64_t> slab_magics_;
};

}  // namespace ckmalloc

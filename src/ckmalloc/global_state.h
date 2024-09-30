#pragma once

#include "src/ckmalloc/freelist.h"
#include "src/ckmalloc/main_allocator.h"
#include "src/ckmalloc/metadata_manager.h"
#include "src/ckmalloc/slab_manager.h"
#include "src/ckmalloc/slab_map.h"
#include "src/ckmalloc/small_allocator.h"

namespace ckmalloc {

class GlobalState {
  friend class CkMalloc;

 public:
  GlobalState(void* metadata_heap, void* metadata_heap_end, void* user_heap);

  SlabMap* SlabMap() {
    return &slab_map_;
  }

  SlabManager* SlabManager() {
    return &slab_manager_;
  }

  MetadataManager* MetadataManager() {
    return &metadata_manager_;
  }

  MainAllocator* MainAllocator() {
    return &main_allocator_;
  }

  // These assertions help the compiler avoid redundant memory reads for member
  // pointers to other metadata types.
  void AssertConsistency() {
    CK_ASSERT_EQ(slab_manager_.slab_map_, &slab_map_);
    CK_ASSERT_EQ(metadata_manager_.slab_map_, &slab_map_);
    CK_ASSERT_EQ(small_alloc_.slab_map_, &slab_map_);
    CK_ASSERT_EQ(small_alloc_.slab_manager_, &slab_manager_);
    CK_ASSERT_EQ(small_alloc_.freelist_, &freelist_);
    CK_ASSERT_EQ(large_alloc_.slab_map_, &slab_map_);
    CK_ASSERT_EQ(large_alloc_.slab_manager_, &slab_manager_);
    CK_ASSERT_EQ(large_alloc_.freelist_, &freelist_);
    CK_ASSERT_EQ(main_allocator_.slab_map_, &slab_map_);
    CK_ASSERT_EQ(main_allocator_.slab_manager_, &slab_manager_);
    CK_ASSERT_EQ(main_allocator_.small_alloc_, &small_alloc_);
    CK_ASSERT_EQ(main_allocator_.large_alloc_, &large_alloc_);
  }

 private:
  ckmalloc::SlabMap slab_map_;
  ckmalloc::SlabManager slab_manager_;
  ckmalloc::MetadataManager metadata_manager_;
  ckmalloc::Freelist freelist_;
  ckmalloc::SmallAllocator small_alloc_;
  ckmalloc::LargeAllocator large_alloc_;
  ckmalloc::MainAllocator main_allocator_;
};

}  // namespace ckmalloc

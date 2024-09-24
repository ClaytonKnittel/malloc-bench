#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/common.h"
#include "src/ckmalloc/large_allocator.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/size_class.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_manager.h"
#include "src/ckmalloc/slab_map.h"
#include "src/ckmalloc/small_allocator.h"
#include "src/ckmalloc/sys_alloc.h"
#include "src/ckmalloc/util.h"
#include "src/heap_interface.h"

namespace ckmalloc {

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap,
          SlabManagerInterface SlabManager,
          SmallAllocatorInterface SmallAllocator,
          LargeAllocatorInterface LargeAllocator>
class MainAllocatorImpl {
  friend class GlobalState;
  friend class TestMainAllocator;

 public:
  MainAllocatorImpl(SlabMap* slab_map, SlabManager* slab_manager,
                    SmallAllocator* small_alloc, LargeAllocator* large_alloc)
      : slab_map_(slab_map),
        slab_manager_(slab_manager),
        small_alloc_(small_alloc),
        large_alloc_(large_alloc) {}

  // Allocates a region of memory `user_size` bytes long, returning a pointer to
  // the beginning of the region.
  Void* Alloc(size_t user_size);

  // Re-allocates a region of memory to be `user_size` bytes long, returning a
  // pointer to the beginning of the new region and copying the data from `ptr`
  // over. The returned pointer may equal the `ptr` argument. If `user_size` is
  // larger than the previous size of the region starting at `ptr`, the
  // remaining data after the size of the previous region is uninitialized, and
  // if `user_size` is smaller, the data is truncated.
  Void* Realloc(Void* ptr, size_t user_size);

  // Frees an allocation returned from `Alloc`, allowing that memory to be
  // reused by future `Alloc`s.
  void Free(Void* ptr);

  // Given a pointer to an allocated region, returns the size of the region.
  size_t AllocSize(Void* ptr) const;

  // Given a pointer to an allocated region, returns the size of the region.
  SizeClass AllocSizeClass(Void* ptr) const;

 private:
  Void* AllocMmap(size_t user_size);

  void FreeMmap(MmapSlab* slab, Void* ptr);

  SlabMap* const slab_map_;
  SlabManager* const slab_manager_;
  SmallAllocator* const small_alloc_;
  LargeAllocator* const large_alloc_;
};

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap,
          SlabManagerInterface SlabManager,
          SmallAllocatorInterface SmallAllocator,
          LargeAllocatorInterface LargeAllocator>
Void* MainAllocatorImpl<MetadataAlloc, SlabMap, SlabManager, SmallAllocator,
                        LargeAllocator>::Alloc(size_t user_size) {
  if (IsSmallSize(user_size)) {
    return small_alloc_->AllocSmall(user_size);
  } else if (IsMmapSize(user_size)) {
    return AllocMmap(user_size);
  } else {
    return large_alloc_->AllocLarge(user_size);
  }
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap,
          SlabManagerInterface SlabManager,
          SmallAllocatorInterface SmallAllocator,
          LargeAllocatorInterface LargeAllocator>
Void* MainAllocatorImpl<MetadataAlloc, SlabMap, SlabManager, SmallAllocator,
                        LargeAllocator>::Realloc(Void* ptr, size_t user_size) {
  Slab* slab = slab_map_->FindSlab(PageId::FromPtr(ptr));
  CK_ASSERT_NE(slab->Type(), SlabType::kFree);
  CK_ASSERT_NE(slab->Type(), SlabType::kUnmapped);

  switch (slab->Type()) {
    case SlabType::kSmall: {
      // If this is a small-to-small size reallocation, we can use the
      // specialized realloc in small allocator.
      if (IsSmallSize(user_size)) {
        return small_alloc_->ReallocSmall(slab->ToSmall(), ptr, user_size);
      }

      // Otherwise, we will always need to alloc-copy-free. First allocate the
      // large block.
      Void* new_ptr;
      if (IsMmapSize(user_size)) {
        new_ptr = AllocMmap(user_size);
      } else {
        new_ptr = large_alloc_->AllocLarge(user_size);
      }
      if (new_ptr == nullptr) {
        return nullptr;
      }

      // Then copy user data over. Note that the slice's size will always be
      // smaller than `user_size`, so no need to take the min of the two.
      std::memcpy(new_ptr, ptr, slab->ToSmall()->SizeClass().SliceSize());

      // Free the slice and return the newly allocated block.
      small_alloc_->FreeSmall(slab->ToSmall(), ptr);
      return new_ptr;
    }
    case SlabType::kBlocked:
    case SlabType::kSingleAlloc: {
      if (!IsSmallSize(user_size) && !IsMmapSize(user_size)) {
        return large_alloc_->ReallocLarge(slab->ToLarge(), ptr, user_size);
      }

      // Otherwise, we will always need to alloc-copy-free. First allocate the
      // small slice.
      Void* new_ptr;
      size_t copy_size;
      if (IsMmapSize(user_size)) {
        new_ptr = AllocMmap(user_size);
        if (slab->Type() == SlabType::kBlocked) {
          copy_size = AllocatedBlock::FromUserDataPtr(ptr)->UserDataSize();
        } else {
          copy_size = slab->ToSingleAlloc()->Pages() * kPageSize;
        }
      } else {
        new_ptr = small_alloc_->AllocSmall(user_size);
        copy_size = user_size;
      }
      if (new_ptr == nullptr) {
        return nullptr;
      }

      // Then copy user data over.
      std::memcpy(new_ptr, ptr, copy_size);

      // Free the block and return the newly allocated slice.
      large_alloc_->FreeLarge(slab->ToLarge(), ptr);
      return new_ptr;
    }
    case SlabType::kMmap: {
      MmapSlab* const mmap_slab = slab->ToMmap();
      Void* new_ptr;
      size_t copy_size;
      if (IsMmapSize(user_size)) {
        uint32_t n_pages = CeilDiv(user_size, kPageSize);
        if (n_pages == mmap_slab->Pages()) {
          return ptr;
        }

        new_ptr = AllocMmap(user_size);
        copy_size = std::min(mmap_slab->Pages(), n_pages) * kPageSize;
      } else if (IsSmallSize(user_size)) {
        new_ptr = small_alloc_->AllocSmall(user_size);
        copy_size = user_size;
      } else {
        new_ptr = large_alloc_->AllocLarge(user_size);
        copy_size = user_size;
      }

      // Copy user data over.
      std::memcpy(new_ptr, ptr, copy_size);

      // Free the block and return the newly allocated pointer.
      FreeMmap(mmap_slab, ptr);
      return new_ptr;
    }
    case SlabType::kUnmapped:
    case SlabType::kFree: {
      // Unexpected free/unmapped slab.
      CK_UNREACHABLE("Unexpected free/unmapped slab");
      return nullptr;
    }
  }
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap,
          SlabManagerInterface SlabManager,
          SmallAllocatorInterface SmallAllocator,
          LargeAllocatorInterface LargeAllocator>
void MainAllocatorImpl<MetadataAlloc, SlabMap, SlabManager, SmallAllocator,
                       LargeAllocator>::Free(Void* ptr) {
  Slab* slab = slab_map_->FindSlab(PageId::FromPtr(ptr));
  CK_ASSERT_NE(slab->Type(), SlabType::kFree);
  CK_ASSERT_NE(slab->Type(), SlabType::kUnmapped);

  switch (slab->Type()) {
    case SlabType::kSmall: {
      small_alloc_->FreeSmall(slab->ToSmall(), ptr);
      break;
    }
    case SlabType::kBlocked:
    case SlabType::kSingleAlloc: {
      large_alloc_->FreeLarge(slab->ToLarge(), ptr);
      break;
    }
    case SlabType::kMmap: {
      FreeMmap(slab->ToMmap(), ptr);
      break;
    }
    case SlabType::kUnmapped:
    case SlabType::kFree: {
      // Unexpected free/unmapped slab.
      CK_UNREACHABLE("Unexpected free/unmapped slab");
      break;
    }
  }
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap,
          SlabManagerInterface SlabManager,
          SmallAllocatorInterface SmallAllocator,
          LargeAllocatorInterface LargeAllocator>
size_t MainAllocatorImpl<MetadataAlloc, SlabMap, SlabManager, SmallAllocator,
                         LargeAllocator>::AllocSize(Void* ptr) const {
  SizeClass size_class = AllocSizeClass(ptr);
  if (size_class != SizeClass::Nil()) {
    return size_class.SliceSize();
  }

  PageId page_id = PageId::FromPtr(ptr);
  Slab* slab = slab_map_->FindSlab(page_id);
  CK_ASSERT_NE(slab, nullptr);
  CK_ASSERT_NE(slab->Type(), SlabType::kFree);
  CK_ASSERT_NE(slab->Type(), SlabType::kSmall);
  CK_ASSERT_NE(slab->Type(), SlabType::kUnmapped);

  // TODO: do these in respective specialized allocators.
  switch (slab->Type()) {
    case SlabType::kBlocked: {
      return AllocatedBlock::FromUserDataPtr(ptr)->UserDataSize();
    }
    case SlabType::kSingleAlloc: {
      return slab->ToSingleAlloc()->Pages() * kPageSize;
    }
    case SlabType::kMmap: {
      return slab->ToMmap()->Pages() * kPageSize;
    }
    case SlabType::kUnmapped:
    case SlabType::kFree:
    case SlabType::kSmall: {
      // Unexpected free/unmapped slab.
      CK_UNREACHABLE("Unexpected free/small/unmapped slab");
      return 0;
    }
  }
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap,
          SlabManagerInterface SlabManager,
          SmallAllocatorInterface SmallAllocator,
          LargeAllocatorInterface LargeAllocator>
SizeClass MainAllocatorImpl<MetadataAlloc, SlabMap, SlabManager, SmallAllocator,
                            LargeAllocator>::AllocSizeClass(Void* ptr) const {
  PageId page_id = PageId::FromPtr(ptr);
  return slab_map_->FindSizeClass(page_id);
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap,
          SlabManagerInterface SlabManager,
          SmallAllocatorInterface SmallAllocator,
          LargeAllocatorInterface LargeAllocator>
Void* MainAllocatorImpl<MetadataAlloc, SlabMap, SlabManager, SmallAllocator,
                        LargeAllocator>::AllocMmap(size_t user_size) {
  Slab* slab = MetadataAlloc::SlabAlloc();
  if (slab == nullptr) {
    return nullptr;
  }

  uint32_t n_pages = CeilDiv(user_size, kPageSize);
  bench::Heap* heap = TestSysAlloc::Instance()->Mmap(
      /*start_hint=*/nullptr, n_pages * kPageSize);
  if (heap == nullptr) {
    MetadataAlloc::SlabFree(static_cast<AllocatedSlab*>(slab));
    return nullptr;
  }
  heap->sbrk(n_pages * kPageSize);

  void* heap_start = heap->Start();
  PageId start_id = PageId::FromPtr(heap_start);
  MmapSlab* mmap_slab = slab->Init<MmapSlab>(start_id, n_pages, heap);

  slab_map_->AllocatePath(start_id, start_id);
  slab_map_->Insert(start_id, mmap_slab, SizeClass::Nil());
  return static_cast<Void*>(heap_start);
}

template <MetadataAllocInterface MetadataAlloc, SlabMapInterface SlabMap,
          SlabManagerInterface SlabManager,
          SmallAllocatorInterface SmallAllocator,
          LargeAllocatorInterface LargeAllocator>
void MainAllocatorImpl<MetadataAlloc, SlabMap, SlabManager, SmallAllocator,
                       LargeAllocator>::FreeMmap(MmapSlab* slab, Void* ptr) {
  CK_ASSERT_EQ(slab->StartId().PageStart(), ptr);
  TestSysAlloc::Instance()->Munmap(slab->Heap());
}

using MainAllocator =
    MainAllocatorImpl<GlobalMetadataAlloc, SlabMap, SlabManager, SmallAllocator,
                      LargeAllocator>;

}  // namespace ckmalloc

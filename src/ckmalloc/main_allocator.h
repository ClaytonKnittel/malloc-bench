#pragma once

#include <cstddef>
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
#include "src/ckmalloc/util.h"

namespace ckmalloc {

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager,
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

 private:
  SlabMap* const slab_map_;
  SlabManager* const slab_manager_;
  SmallAllocator* const small_alloc_;
  LargeAllocator* const large_alloc_;
};

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager,
          SmallAllocatorInterface SmallAllocator,
          LargeAllocatorInterface LargeAllocator>
Void* MainAllocatorImpl<SlabMap, SlabManager, SmallAllocator,
                        LargeAllocator>::Alloc(size_t user_size) {
  if (IsSmallSize(user_size)) {
    return small_alloc_->AllocSmall(user_size);
  } else {
    return large_alloc_->AllocLarge(user_size);
  }
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager,
          SmallAllocatorInterface SmallAllocator,
          LargeAllocatorInterface LargeAllocator>
Void* MainAllocatorImpl<SlabMap, SlabManager, SmallAllocator,
                        LargeAllocator>::Realloc(Void* ptr, size_t user_size) {
  Slab* slab = slab_map_->FindSlab(PageId::FromPtr(ptr));
  CK_ASSERT_NE(slab->Type(), SlabType::kFree);
  CK_ASSERT_NE(slab->Type(), SlabType::kUnmapped);

  switch (slab->Type()) {
    case SlabType::kSmall: {
      // If this is a small-to-small size reallocation, we can use the
      // specialized realloc in small allocator.
      if (user_size <= kMaxSmallSize) {
        return small_alloc_->ReallocSmall(slab->ToSmall(), ptr, user_size);
      }

      // Otherwise, we will always need to alloc-copy-free. First allocate the
      // large block.
      Void* new_ptr = large_alloc_->AllocLarge(user_size);
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
      if (user_size > kMaxSmallSize) {
        return large_alloc_->ReallocLarge(slab->ToLarge(), ptr, user_size);
      }

      // Otherwise, we will always need to alloc-copy-free. First allocate the
      // small slice.
      Void* new_ptr = small_alloc_->AllocSmall(user_size);
      if (new_ptr == nullptr) {
        return nullptr;
      }

      // Then copy user data over. Note that the slice's size will always be
      // smaller than `block`'s size, so no need to take the min of the two.
      std::memcpy(new_ptr, ptr, user_size);

      // Free the block and return the newly allocated slice.
      large_alloc_->FreeLarge(slab->ToLarge(), ptr);
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

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager,
          SmallAllocatorInterface SmallAllocator,
          LargeAllocatorInterface LargeAllocator>
void MainAllocatorImpl<SlabMap, SlabManager, SmallAllocator,
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
    case SlabType::kUnmapped:
    case SlabType::kFree: {
      // Unexpected free/unmapped slab.
      CK_UNREACHABLE("Unexpected free/unmapped slab");
      break;
    }
  }
}

template <SlabMapInterface SlabMap, SlabManagerInterface SlabManager,
          SmallAllocatorInterface SmallAllocator,
          LargeAllocatorInterface LargeAllocator>
size_t MainAllocatorImpl<SlabMap, SlabManager, SmallAllocator,
                         LargeAllocator>::AllocSize(Void* ptr) const {
  PageId page_id = PageId::FromPtr(ptr);
  SizeClass size_class = slab_map_->FindSizeClass(page_id);
  if (size_class != SizeClass::Nil()) {
    return size_class.SliceSize();
  }

  Slab* slab = slab_map_->FindSlab(page_id);
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
    case SlabType::kUnmapped:
    case SlabType::kFree:
    case SlabType::kSmall: {
      // Unexpected free/unmapped slab.
      CK_UNREACHABLE("Unexpected free/small/unmapped slab");
      return 0;
    }
  }
}

using MainAllocator =
    MainAllocatorImpl<SlabMap, SlabManager, SmallAllocator, LargeAllocator>;

}  // namespace ckmalloc

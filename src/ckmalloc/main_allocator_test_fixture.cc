#include "src/ckmalloc/main_allocator_test_fixture.h"

#include <bit>
#include <cstddef>

#include "absl/status/status.h"
#include "util/absl_util.h"

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/common.h"
#include "src/ckmalloc/freelist.h"
#include "src/ckmalloc/size_class.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/testlib.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

TestMainAllocator::TestMainAllocator(MainAllocatorFixture* test_fixture,
                                     TestSlabMap* slab_map,
                                     TestSlabManager* slab_manager,
                                     TestSmallAllocator* small_alloc,
                                     TestLargeAllocator* large_alloc)
    : test_fixture_(test_fixture),
      main_allocator_(slab_map, slab_manager, small_alloc, large_alloc) {}

Freelist& TestMainAllocator::Freelist() {
  return test_fixture_->Freelist();
}

Void* TestMainAllocator::Alloc(size_t user_size) {
  Void* alloc = main_allocator_.Alloc(user_size);
  if (alloc == nullptr) {
    return nullptr;
  }

  uint64_t magic = test_fixture_->rng_.GenRand64();
  MainAllocatorFixture::FillMagic(alloc, user_size, magic);

  auto [it, inserted] =
      test_fixture_->allocations_.insert({ alloc, { user_size, magic } });
  CK_ASSERT_TRUE(inserted);

  return alloc;
}

Void* TestMainAllocator::Realloc(Void* ptr, size_t user_size) {
  auto it = test_fixture_->allocations_.find(ptr);
  CK_ASSERT_FALSE(it == test_fixture_->allocations_.end());
  auto [old_size, magic] = it->second;

  Void* new_alloc = main_allocator_.Realloc(ptr, user_size);
  if (new_alloc == nullptr) {
    // Old memory block will not be freed, so no need to remove it from the
    // `allocations_` table.
    return nullptr;
  }

  if (user_size > old_size) {
    MainAllocatorFixture::FillMagic(PtrAdd(new_alloc, old_size),
                                    user_size - old_size,
                                    std::rotr(magic, (old_size % 8) * 8));
  }

  test_fixture_->allocations_.erase(it);

  auto [it2, inserted] =
      test_fixture_->allocations_.insert({ new_alloc, { user_size, magic } });
  CK_ASSERT_TRUE(inserted);

  return new_alloc;
}

void TestMainAllocator::Free(Void* ptr) {
  auto it = test_fixture_->allocations_.find(ptr);
  CK_ASSERT_FALSE(it == test_fixture_->allocations_.end());

  test_fixture_->allocations_.erase(it);
  return main_allocator_.Free(ptr);
}

size_t TestMainAllocator::AllocSize(Void* ptr) {
  return main_allocator_.AllocSize(ptr);
}

absl::Status MainAllocatorFixture::ValidateHeap() {
  // Validate that allocations do not overlap and that their magic numbers are
  // intact.
  for (auto it = allocations_.cbegin(); it != allocations_.cend();) {
    auto [alloc, meta] = *it;
    auto [size, magic] = meta;

    // Check for overlapping allocations
    auto next_it = it;
    ++next_it;
    if (next_it != allocations_.cend()) {
      auto [alloc2, meta2] = *next_it;
      auto [size2, magic2] = meta2;

      if (alloc2 < PtrAdd(alloc, size)) {
        return FailedTest(
            "Allocation %p of size %zu overlaps with allocation %p of size %zu",
            alloc, size, alloc2, size2);
      }
    }

    // Check magic bytes of block.
    RETURN_IF_ERROR(CheckMagic(alloc, size, magic));

    MappedSlab* slab = SlabMap().FindSlab(PageId::FromPtr(alloc));
    size_t derived_size = MainAllocator().AllocSize(alloc);
    size_t aligned_size;
    switch (slab->Type()) {
      case SlabType::kSingleAlloc: {
        aligned_size = AlignUp(size, kPageSize);
        break;
      }
      case SlabType::kBlocked: {
        aligned_size =
            Block::UserSizeForBlockSize(Block::BlockSizeForUserSize(size));
        break;
      }
      case SlabType::kSmall: {
        aligned_size = SizeClass::FromUserDataSize(size).SliceSize();
        break;
      }
      case SlabType::kFree:
      case SlabType::kUnmapped: {
        return FailedTest("Unexpected non-allocated slab %v", *slab);
      }
    }
    if (aligned_size != derived_size) {
      if (slab->Type() != SlabType::kBlocked || aligned_size > derived_size ||
          aligned_size + Block::kMinBlockSize <= derived_size) {
        return FailedTest(
            "Allocated block at %p of size %zu in %v has the wrong size when "
            "looked up with MainAllocator::AllocSize: found %zu, expected %zu "
            "- %zu",
            alloc, size, *slab, derived_size, aligned_size,
            slab->Type() == SlabType::kBlocked
                ? aligned_size + Block::kMinBlockSize
                : aligned_size);
      }
    }

    it = next_it;
  }

  return absl::OkStatus();
}

absl::Status MainAllocatorFixture::ValidateEmpty() {
  if (!allocations_.empty()) {
    return FailedTest("Expected empty allocations set");
  }

  return absl::OkStatus();
}

/* static */
void MainAllocatorFixture::FillMagic(void* allocation, size_t size,
                                     uint64_t magic) {
  uint8_t* start = reinterpret_cast<uint8_t*>(allocation);
  uint8_t* end = start + size;

  for (uint8_t* ptr = start; ptr != end; ptr++) {
    uint32_t byte = static_cast<uint32_t>((ptr - start) % 8);
    *ptr = (magic >> (8 * byte)) & 0xff;
  }
}

absl::Status MainAllocatorFixture::CheckMagic(void* allocation, size_t size,
                                              uint64_t magic) {
  uint8_t* start = reinterpret_cast<uint8_t*>(allocation);
  uint8_t* end = start + size;

  for (uint8_t* ptr = start; ptr != end; ptr++) {
    uint32_t byte = static_cast<uint32_t>((ptr - start) % 8);
    if (*ptr != ((magic >> (8 * byte)) & 0xff)) {
      return FailedTest("Allocation %p was dirtied starting from offset %lu",
                        allocation, ptr - start);
    }
  }

  return absl::OkStatus();
}

}  // namespace ckmalloc

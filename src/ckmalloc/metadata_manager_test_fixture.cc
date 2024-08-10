#include "src/ckmalloc/metadata_manager_test_fixture.h"

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "util/absl_util.h"

#include "src/ckmalloc/slab_manager_test_fixture.h"
#include "src/ckmalloc/testlib.h"
#include "src/ckmalloc/util.h"
#include "src/rng.h"

namespace ckmalloc {

using TestMetadataManager = MetadataManagerTest::TestMetadataManager;

TestMetadataManager::TestMetadataManager(MetadataManagerTest* test_fixture,
                                         TestSlabMap* slab_map,
                                         TestSlabManager* slab_manager)
    : test_fixture_(test_fixture), metadata_manager_(slab_map, slab_manager) {}

void* TestMetadataManager::Alloc(size_t size, size_t alignment) {
  void* block = metadata_manager_.Alloc(size, alignment);
  if (block == nullptr) {
    return nullptr;
  }

  auto [it, inserted] =
      test_fixture_->allocated_blocks_.insert({ block, size });
  CK_ASSERT(inserted);

  return block;
}

Slab* TestMetadataManager::NewSlabMeta() {
  return metadata_manager_.NewSlabMeta();
}

void TestMetadataManager::FreeSlabMeta(Slab* slab) {
  return metadata_manager_.FreeSlabMeta(slab);
}

absl::StatusOr<size_t> MetadataManagerTest::SlabMetaFreelistLength() const {
  constexpr size_t kMaxReasonableLength = 10000;
  size_t length = 0;
  for (Slab* free_slab = metadata_manager_.Underlying().last_free_slab_;
       free_slab != nullptr && length < kMaxReasonableLength;
       free_slab = free_slab->NextUnmappedSlab(), length++)
    ;

  return length == kMaxReasonableLength
             ? absl::FailedPreconditionError(
                   "Slab metadata freelist appears to have a cycle")
             : absl::StatusOr<size_t>(length);
}

absl::StatusOr<void*> MetadataManagerTest::Alloc(size_t size,
                                                 size_t alignment) {
  void* result = metadata_manager_.Alloc(size, alignment);
  if (result == nullptr) {
    return nullptr;
  }

  // Check that the pointer is aligned relative to the heap start. The heap will
  // be page-aligned in production, but may not be in tests.
  if (((reinterpret_cast<intptr_t>(result) -
        reinterpret_cast<intptr_t>(Heap().Start())) &
       (alignment - 1)) != 0) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Pointer returned from Alloc not aligned properly: "
                        "pointer %p, size %zu, alignment %zu",
                        result, size, alignment));
  }

  if (result < Heap().Start() ||
      static_cast<uint8_t*>(result) + size > Heap().End()) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Block allocated outside range of heap: returned %p of "
                        "size %zu, heap ranges from %p to %p",
                        result, size, Heap().Start(), Heap().End()));
  }

  for (const auto& [ptr, ptr_size] : allocated_blocks_) {
    // Don't check for collision with ourselves.
    if (ptr == result) {
      continue;
    }

    if (ptr < static_cast<uint8_t*>(result) + size &&
        result < static_cast<uint8_t*>(ptr) + ptr_size) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Allocated block overlaps with already allocated block: returned %p "
          "of size %zu, overlaps with %p of size %zu",
          result, size, ptr, ptr_size));
    }
  }

  uint64_t magic = rng_.GenRand64();
  FillMagic(result, size, magic);
  block_magics_.insert({ result, magic });

  return result;
}

/* static */
void MetadataManagerTest::FillMagic(void* block, size_t size, uint64_t magic) {
  uint8_t* start = reinterpret_cast<uint8_t*>(block);

  for (size_t i = 0; i < size; i++) {
    uint8_t magic_byte = (magic >> ((i % 8) * 8)) & 0xff;
    start[i] = magic_byte;
  }
}

/* static */
absl::Status MetadataManagerTest::CheckMagic(void* block, size_t size,
                                             uint64_t magic) {
  uint8_t* start = reinterpret_cast<uint8_t*>(block);

  for (size_t i = 0; i < size; i++) {
    uint8_t magic_byte = (magic >> ((i % 8) * 8)) & 0xff;
    start[i] = magic_byte;
    if (start[i] != magic_byte) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Allocated block %p of size %zu was dirtied starting from offset %zu",
          start, size, i));
    }
  }

  return absl::OkStatus();
}

absl::Status MetadataManagerTest::ValidateHeap() {
  RETURN_IF_ERROR(SlabManagerTest::ValidateHeap());

  for (const auto& [block, size] : allocated_blocks_) {
  }

  for (const auto& [block, magic] : block_magics_) {
    auto it = allocated_blocks_.find(block);
    CK_ASSERT(it != allocated_blocks_.end());
    RETURN_IF_ERROR(CheckMagic(block, it->second, magic));
  }

  return absl::OkStatus();
}

}  // namespace ckmalloc

#include "src/ckmalloc/metadata_manager_test_fixture.h"

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "util/absl_util.h"

#include "src/ckmalloc/testlib.h"
#include "src/rng.h"

namespace ckmalloc {

absl::StatusOr<size_t> MetadataManagerTest::SlabMetaFreelistLength() const {
  constexpr size_t kMaxReasonableLength = 10000;
  size_t length = 0;
  for (Slab* free_slab = metadata_manager_.last_free_slab_;
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
        reinterpret_cast<intptr_t>(heap_.Start())) &
       (alignment - 1)) != 0) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Pointer returned from Alloc not aligned properly: "
                        "pointer %p, size %zu, alignment %zu",
                        result, size, alignment));
  }

  if (result < heap_.Start() ||
      static_cast<uint8_t*>(result) + size > heap_.End()) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Block allocated outside range of heap: returned %p of "
                        "size %zu, heap ranges from %p to %p",
                        result, size, heap_.Start(), heap_.End()));
  }

  for (const auto& [ptr, meta] : allocated_blocks_) {
    if (ptr < static_cast<uint8_t*>(result) + size &&
        result < static_cast<uint8_t*>(ptr) + meta.first) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Allocated block overlaps with already allocated block: returned %p "
          "of size %zu, overlaps with %p of size %zu",
          result, size, ptr, meta.first));
    }
  }

  uint64_t magic = rng_.GenRand64();
  FillMagic(result, size, magic);

  allocated_blocks_.insert({ result, { size, magic } });

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
  if (Heap().Size() % kPageSize != 0) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Expected heap size to be a multiple of page size, but was %zu",
        Heap().Size()));
  }

  for (const auto& [block, meta] : allocated_blocks_) {
    const auto& [size, magic] = meta;
    RETURN_IF_ERROR(CheckMagic(block, size, magic));
  }

  return absl::OkStatus();
}

}  // namespace ckmalloc

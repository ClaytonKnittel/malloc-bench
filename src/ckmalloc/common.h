#pragma once

#include <cstddef>
#include <cstdint>

#include "src/singleton_heap.h"

namespace ckmalloc {

static constexpr uint32_t kPageShift = 12;
// The size of slabs in bytes.
static constexpr size_t kPageSize = 1 << kPageShift;

constexpr uint32_t kHeapSizeShift = 29;
// NOLINTNEXTLINE(google-readability-casting)
static_assert(bench::SingletonHeap::kHeapSize == (size_t(1) << kHeapSizeShift));

using AllocFn = void* (*) (size_t size, size_t alignment);

// The following methods are defined in state.cc, but we export them here to
// avoid circular dependencies in the code.

extern class Slab* SlabMetadataAlloc();
extern void SlabMetadataFree(class Slab* slab);

extern void* MetadataAlloc(size_t size, size_t alignment);

}  // namespace ckmalloc

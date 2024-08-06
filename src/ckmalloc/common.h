#pragma once

#include <cstddef>

namespace ckmalloc {

using AllocFn = void* (*) (size_t size, size_t alignment);

// This method is defined in state.cc, but we can't depend on :state due to
// circular dependencies.
extern void* MetadataAlloc(size_t size, size_t alignment);

}  // namespace ckmalloc

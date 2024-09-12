#include "src/allocator_interface.h"

#include <optional>

#include "src/heap_interface.h"
#include "src/mmap_heap.h"

namespace bench {

namespace {

std::optional<MMapHeap> heap;

}

Heap* g_heap = nullptr;

void initialize() {
  auto res = MMapHeap::New(kHeapSize);
  heap.emplace(std::move(res.value()));
  g_heap = &heap.value();
}

}  // namespace bench

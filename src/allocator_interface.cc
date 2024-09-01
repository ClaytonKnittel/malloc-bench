#include "src/allocator_interface.h"

#include "src/heap_factory.h"

namespace bench {

HeapFactory* g_heap_factory = nullptr;

size_t g_heap_idx = 0;

}  // namespace bench

#include "src/mmap_heap_factory.h"

#include <memory>

#include "absl/status/statusor.h"
#include "util/absl_util.h"

#include "src/heap_interface.h"
#include "src/mmap_heap.h"

namespace bench {

absl::StatusOr<std::unique_ptr<Heap>> MMapHeapFactory::MakeHeap(size_t size) {
  DEFINE_OR_RETURN(MMapHeap, heap, MMapHeap::New(size));
  return std::make_unique<MMapHeap>(std::move(heap));
}

}  // namespace bench

#include "src/heap_factory.h"

#include <memory>

#include "util/absl_util.h"

#include "src/heap_interface.h"

namespace bench {

absl::StatusOr<Heap*> HeapFactory::NewInstance(size_t size) {
  DEFINE_OR_RETURN(std::unique_ptr<Heap>, heap, MakeHeap(size));
  Heap* heap_ptr = heap.get();
  heaps_.emplace(std::move(heap));
  return heap_ptr;
}

const absl::flat_hash_set<std::unique_ptr<Heap>>& HeapFactory::Instances()
    const {
  return heaps_;
}

void HeapFactory::Reset() {
  heaps_.clear();
}

}  // namespace bench

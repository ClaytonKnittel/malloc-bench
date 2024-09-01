#pragma once

#include <cstddef>
#include <vector>

#include "absl/status/statusor.h"

#include "src/heap_interface.h"
#include "src/mmap_heap.h"

namespace bench {

class HeapFactory {
 public:
  HeapFactory() = default;
  HeapFactory(HeapFactory&&) = default;

  // Allocates a new heap of the requested size, returning the index of the new
  // heap and a pointer to it.
  absl::StatusOr<std::pair<size_t, Heap*>> NewInstance(size_t size);

  // Returns the heap instance at index `idx`.
  Heap* Instance(size_t idx);

  const std::vector<MMapHeap>& Instances() const;

  // Clears the heap factory and deletes all allocated heaps.
  void Reset();

 private:
  std::vector<MMapHeap> heaps_;
};

}  // namespace bench

#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "absl/status/statusor.h"

#include "src/heap_interface.h"

namespace bench {

class HeapFactory {
 public:
  HeapFactory() = default;
  HeapFactory(HeapFactory&&) = default;
  virtual ~HeapFactory() = default;

  // Allocates a new heap of the requested size, returning the index of the new
  // heap and a pointer to it.
  absl::StatusOr<std::pair<size_t, Heap*>> NewInstance(size_t size);

  // Returns the heap instance at index `idx`.
  Heap* Instance(size_t idx);
  const Heap* Instance(size_t idx) const;

  const std::vector<std::unique_ptr<Heap>>& Instances() const;

  // Clears the heap factory and deletes all allocated heaps.
  void Reset();

 protected:
  virtual absl::StatusOr<std::unique_ptr<Heap>> MakeHeap(size_t size) = 0;

 private:
  std::vector<std::unique_ptr<Heap>> heaps_;
};

}  // namespace bench

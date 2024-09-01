#pragma once

#include <cstddef>

#include "absl/status/statusor.h"

#include "src/heap_interface.h"

namespace bench {

class MMapHeap : public Heap {
 public:
  MMapHeap(MMapHeap&&) = default;
  ~MMapHeap() override;

  static absl::StatusOr<MMapHeap> New(size_t size);

 private:
  MMapHeap(void* heap_start, size_t size);
};

}  // namespace bench

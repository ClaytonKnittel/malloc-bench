#pragma once

#include <memory>

#include "absl/status/statusor.h"

#include "src/heap_factory.h"
#include "src/heap_interface.h"

namespace bench {

class MMapHeapFactory : public HeapFactory {
 public:
  ~MMapHeapFactory() override = default;

 protected:
  absl::StatusOr<std::unique_ptr<Heap>> MakeHeap(size_t size) override;
};

}  // namespace bench

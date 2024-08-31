#include "src/singleton_heap.h"

#include <cerrno>
#include <cstring>
#include <sys/mman.h>

#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"

#include "src/heap_interface.h"

namespace bench {

SingletonHeap::~SingletonHeap() {
  if (Start() != nullptr) {
    int result = munmap(Start(), kHeapSize);
    if (result == -1) {
      std::cerr << "Failed to unmap heap: " << strerror(errno) << std::endl;
    }
  }
}

/* static */
SingletonHeap* SingletonHeap::GlobalInstance() {
  return &global_heap_;
}

SingletonHeap::SingletonHeap(void* heap_start, size_t size)
    : Heap(heap_start, size) {}

/* static */
absl::StatusOr<SingletonHeap> SingletonHeap::Initialize() {
  void* heap_start = mmap(nullptr, kHeapSize, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (heap_start == MAP_FAILED) {
    return absl::InternalError(
        absl::StrFormat("Failed to mmap region: %s", strerror(errno)));
  }

  return SingletonHeap(heap_start, kHeapSize);
}

SingletonHeap SingletonHeap::global_heap_ = []() {
  auto result = SingletonHeap::Initialize();
  if (!result.ok()) {
    std::cerr << "Failed to initialize heap: " << result.status() << std::endl;
    exit(-1);
  }

  return std::move(result.value());
}();

}  // namespace bench

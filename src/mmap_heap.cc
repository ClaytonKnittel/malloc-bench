#include "src/mmap_heap.h"

#include <cerrno>
#include <cstring>
#include <sys/mman.h>

#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"

#include "src/heap_interface.h"

namespace bench {

MMapHeap::~MMapHeap() {
  if (Start() != nullptr) {
    int result = munmap(Start(), MaxSize());
    if (result == -1) {
      std::cerr << "Failed to unmap heap: " << strerror(errno) << std::endl;
    }
  }
}

MMapHeap::MMapHeap(void* heap_start, size_t size) : Heap(heap_start, size) {}

/* static */
absl::StatusOr<MMapHeap> MMapHeap::NewInstance(size_t size) {
  void* heap_start = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  if (heap_start == MAP_FAILED) {
    return absl::InternalError(absl::StrFormat(
        "Failed to mmap size %zu region: %s", size, strerror(errno)));
  }

  return MMapHeap(heap_start, size);
}

}  // namespace bench

#include "src/fake_heap.h"

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <sys/mman.h>

#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"

namespace bench {

FakeHeap::~FakeHeap() {
  int result = munmap(heap_start_, kHeapSize);
  if (result == -1) {
    std::cerr << "Failed to unmap heap: " << strerror(errno) << std::endl;
  }
}

/* static */
absl::StatusOr<FakeHeap> FakeHeap::Initialize() {
  void* heap_start = mmap(nullptr, FakeHeap::kHeapSize, PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (heap_start == MAP_FAILED) {
    return absl::InternalError(
        absl::StrFormat("Failed to mmap 100MB region: %s", strerror(errno)));
  }

  return FakeHeap(heap_start);
}

void* FakeHeap::sbrk(intptr_t increment) {
  void* old_heap_end = heap_end_;

  if (increment < 0) {
    errno = EINVAL;
    return nullptr;
  }
  if ((static_cast<uint8_t*>(old_heap_end) -
       static_cast<uint8_t*>(heap_start_)) +
          increment >
      kHeapSize) {
    errno = ENOMEM;
    return nullptr;
  }

  heap_end_ = static_cast<uint8_t*>(old_heap_end) + increment;
  return old_heap_end;
}

FakeHeap::FakeHeap(void* heap_start)
    : heap_start_(heap_start), heap_end_(heap_start) {}

void* FakeHeap::Reset() {
  heap_end_ = heap_start_;
  return heap_start_;
}

}  // namespace bench

#include "src/fake_heap.h"

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <sys/mman.h>

#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"

namespace bench {

FakeHeap::FakeHeap(FakeHeap&& heap) noexcept
    : heap_start_(heap.heap_start_), heap_end_(heap.heap_end_) {
  heap.heap_start_ = nullptr;
  heap.heap_end_ = nullptr;
}

FakeHeap::~FakeHeap() {
  if (heap_start_ != nullptr) {
    int result = munmap(heap_start_, kHeapSize);
    if (result == -1) {
      std::cerr << "Failed to unmap heap: " << strerror(errno) << std::endl;
    }
  }
}

FakeHeap& FakeHeap::operator=(FakeHeap&& heap) noexcept {
  FakeHeap(std::move(heap)).swap(*this);
  return *this;
}

void FakeHeap::swap(FakeHeap& other) noexcept {
  std::swap(heap_start_, other.heap_start_);
  std::swap(heap_end_, other.heap_end_);
}

/* static */
FakeHeap* FakeHeap::GlobalInstance() {
  return &global_heap_;
}

void* FakeHeap::Reset() {
  heap_end_ = heap_start_;
  return heap_start_;
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

/* static */
absl::StatusOr<FakeHeap> FakeHeap::Initialize() {
  void* heap_start = mmap(nullptr, FakeHeap::kHeapSize, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (heap_start == MAP_FAILED) {
    return absl::InternalError(
        absl::StrFormat("Failed to mmap 100MB region: %s", strerror(errno)));
  }

  return FakeHeap(heap_start);
}

FakeHeap FakeHeap::global_heap_ = []() {
  auto result = FakeHeap::Initialize();
  if (!result.ok()) {
    std::cerr << "Failed to initialize heap: " << result.status() << std::endl;
    exit(-1);
  }

  return std::move(result.value());
}();

}  // namespace bench

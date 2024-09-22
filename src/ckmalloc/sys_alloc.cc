#include "src/ckmalloc/sys_alloc.h"

#include <memory>

#include "src/heap_interface.h"

namespace ckmalloc {

std::unique_ptr<TestSysAlloc> TestSysAlloc::instance_;

TestSysAlloc::TestSysAlloc(bench::HeapFactory* heap_factory)
    : heap_factory_(heap_factory) {}

/* static */
TestSysAlloc* TestSysAlloc::NewInstance(bench::HeapFactory* heap_factory) {
  instance_ = std::make_unique<TestSysAlloc>(heap_factory);
  return instance_.get();
}

/* static */
TestSysAlloc* TestSysAlloc::Instance() {
  return instance_.get();
}

bench::Heap* TestSysAlloc::Mmap(void* start_hint, size_t size) {
  (void) start_hint;
  auto result = heap_factory_->NewInstance(size);
  if (!result.ok()) {
    std::cerr << "Mmap failed: " << result.status() << std::endl;
    return nullptr;
  }

  return result.value();
}

void TestSysAlloc::Munmap(bench::Heap* heap) {
  auto result = heap_factory_->DeleteInstance(heap);
  if (!result.ok()) {
    std::cerr << "Heap delete failed: " << result << std::endl;
  }
}

}  // namespace ckmalloc

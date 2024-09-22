#pragma once

#include <memory>

#include "src/heap_factory.h"
#include "src/heap_interface.h"

namespace ckmalloc {

class TestSysAlloc {
 public:
  explicit TestSysAlloc(bench::HeapFactory* heap_factory);

  static TestSysAlloc* NewInstance(bench::HeapFactory* heap_factory);

  static TestSysAlloc* Instance();

  bench::Heap* Mmap(void* start_hint, size_t size);

  void Munmap(bench::Heap* heap);

 private:
  static std::unique_ptr<TestSysAlloc> instance_;

  bench::HeapFactory* heap_factory_;
};

}  // namespace ckmalloc

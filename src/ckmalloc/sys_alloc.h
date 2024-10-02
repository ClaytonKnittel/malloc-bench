#pragma once

#include <cstddef>

namespace ckmalloc {

enum class HeapType {
  kMetadataHeap,
  kUserHeap,
  kMmapAllocHeap,
};

class SysAlloc {
 public:
  static SysAlloc* Instance();

  virtual void* Mmap(void* start_hint, size_t size, HeapType type) = 0;

  virtual void Munmap(void* ptr, size_t size) = 0;

  virtual void Sbrk(void* heap_start, size_t increment, void* current_end) = 0;

 protected:
  static SysAlloc* instance_;
};

class RealSysAlloc : public SysAlloc {
 public:
  static void UseRealSysAlloc();

  void* Mmap(void* start_hint, size_t size, HeapType type) override;

  void Munmap(void* ptr, size_t size) override;

  void Sbrk(void* heap_start, size_t increment, void* current_end) override;

 private:
  static RealSysAlloc instance_;
};

}  // namespace ckmalloc

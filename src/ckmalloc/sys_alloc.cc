#include "src/ckmalloc/sys_alloc.h"

#include <ostream>
#include <sys/mman.h>

namespace ckmalloc {

SysAlloc* SysAlloc::instance_ = nullptr;

RealSysAlloc RealSysAlloc::instance_;

std::ostream& operator<<(std::ostream& ostr, HeapType heap_type) {
  switch (heap_type) {
    case HeapType::kMetadataHeap:
      return ostr << "kMetadataHeap";
    case HeapType::kUserHeap:
      return ostr << "kUserHeap";
    case HeapType::kMmapAllocHeap:
      return ostr << "kMmapAllocHeap";
  }
}

/* static */
SysAlloc* SysAlloc::Instance() {
  return instance_;
}

/* static */
void RealSysAlloc::UseRealSysAlloc() {
  SysAlloc::instance_ = &instance_;
}

void* RealSysAlloc::Mmap(void* start_hint, size_t size, HeapType type) {
  (void) type;
  return ::mmap(start_hint, size, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
}

void RealSysAlloc::Munmap(void* ptr, size_t size) {
  ::munmap(ptr, size);
}

void RealSysAlloc::Sbrk(void* heap_start, size_t increment, void* current_end) {
}

}  // namespace ckmalloc

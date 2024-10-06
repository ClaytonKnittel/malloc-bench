#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <mutex>

#include "src/heap_factory.h"
#include "src/jsmalloc/allocator.h"
#include "src/jsmalloc/jsmalloc.h"
#include "src/jsmalloc/util/file_logger.h"

namespace bench {

namespace {

std::mutex g_mutex;
bool g_initialized = false;

}  // namespace

void initialize_heap(HeapFactory& heap_factory) {
  std::lock_guard l(g_mutex);
  static jsmalloc::HeapFactoryAdaptor adaptor(&heap_factory);
  new (&adaptor) jsmalloc::HeapFactoryAdaptor(&heap_factory);
  jsmalloc::initialize_heap(adaptor);
  g_initialized = true;
}

void initialize() {
  if (g_initialized) {
    return;
  }
  g_initialized = true;

  static jsmalloc::MMapMemRegionAllocator allocator;
  jsmalloc::initialize_heap(allocator);
}

void* malloc(size_t size, size_t alignment = 0) {
  std::lock_guard l(g_mutex);
  initialize();

  void* ptr = jsmalloc::malloc(size, alignment);
  DLOG(DEBUG, "malloc(%zu, alignment=%zu) = %p\n", size, alignment, ptr);
  if (ptr == nullptr && size > 0) {
    DLOG(ERROR, "ERROR: malloc(%zu, alignment=%zu) = nullptr\n", size,
         alignment);
  }
  return ptr;
}

void* calloc(size_t nmemb, size_t size) {
  std::lock_guard l(g_mutex);
  initialize();

  void* ptr = jsmalloc::calloc(nmemb, size);
  DLOG(DEBUG, "calloc(%zu, %zu) = %p\n", nmemb, size, ptr);
  if (ptr == nullptr) {
    DLOG(ERROR, "ERROR: calloc(%zu, %zu) = nullptr\n", nmemb, size);
  }
  return ptr;
}

void* realloc(void* ptr, size_t size) {
  std::lock_guard l(g_mutex);
  initialize();

  void* new_ptr = jsmalloc::realloc(ptr, size);
  DLOG(DEBUG, "realloc(%p, %zu) = %p\n", ptr, size, new_ptr);
  if (new_ptr == nullptr) {
    DLOG(ERROR, "ERROR: realloc(%p, %zu) = nullptr\n", ptr, size);
  }
  return new_ptr;
}

void free(void* ptr, size_t size = 0, size_t alignment = 0) {
  std::lock_guard l(g_mutex);
  initialize();

  DLOG(DEBUG, "free(%p)\n", ptr);
  return jsmalloc::free(ptr, size, alignment);
}

size_t get_size(void* ptr) {
  std::lock_guard l(g_mutex);
  initialize();

  return jsmalloc::get_size(ptr);
}

}  // namespace bench

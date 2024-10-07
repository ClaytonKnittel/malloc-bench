#include <cerrno>
#include <features.h>
#include <malloc.h>
#include <new>
#include <unistd.h>

#include "src/allocator_interface.h"

#define ALLOC_ALIAS(fn) __attribute__((alias(#fn), visibility("default")))

// NOLINTBEGIN(bugprone-reserved-identifier, readability-identifier-naming)

extern "C" {

namespace {

inline int posix_memalign_helper(void** ptr, size_t align, size_t size) {
  void* result = bench::malloc(size, align);
  if (result == nullptr) {
    return ENOMEM;
  }
  *ptr = result;
  return 0;
}

}  // namespace

void* __libc_malloc(size_t size) noexcept {
  return bench::malloc(size);
}
void __libc_free(void* ptr) noexcept {
  return bench::free(ptr);
};
void* __libc_realloc(void* ptr, size_t size) noexcept {
  return bench::realloc(ptr, size);
}
void* __libc_calloc(size_t n, size_t size) noexcept {
  return bench::calloc(n, size);
}
extern "C" void __libc_cfree(void* ptr) noexcept {
  bench::free(ptr);
}
void* __libc_memalign(size_t align, size_t s) noexcept {
  return bench::malloc(s, align);
}
void* __libc_valloc(size_t size) noexcept {
  return bench::malloc(size, 4096);
}
void* __libc_pvalloc(size_t size) noexcept {
  return bench::malloc((size + 4095) & ~4096, 4096);
}
int __posix_memalign(void** r, size_t a, size_t s) noexcept {
  return posix_memalign_helper(r, a, s);
}

// We also have to hook libc malloc.  While our work with weak symbols
// should make sure libc malloc is never called in most situations, it
// can be worked around by shared libraries with the DEEPBIND
// environment variable set.  The below hooks libc to call our malloc
// routines even in that situation.  In other situations, this hook
// should never be called.

void* glibc_override_malloc(size_t size, const void* caller) {
  (void) caller;
  return bench::malloc(size);
}
void* glibc_override_realloc(void* ptr, size_t size, const void* caller) {
  (void) caller;
  return bench::realloc(ptr, size);
}
void glibc_override_free(void* ptr, const void* caller) {
  (void) caller;
  bench::free(ptr);
}
void* glibc_override_memalign(size_t align, size_t size, const void* caller) {
  (void) caller;
  return bench::malloc(size, align);
}

// We should be using __malloc_initialize_hook here.  (See
// http://swoolley.org/man.cgi/3/malloc_hook.)  However, this causes weird
// linker errors with programs that link with -static, so instead we just assign
// the vars directly at static-constructor time.  That should serve the same
// effect of making sure the hooks are set before the first malloc call the
// program makes.

// Glibc-2.14 and above make __malloc_hook and friends volatile
#ifndef __MALLOC_HOOK_VOLATILE
#define __MALLOC_HOOK_VOLATILE /**/
#endif

void* (*__MALLOC_HOOK_VOLATILE __malloc_hook)(size_t, const void*) =
    &glibc_override_malloc;
void* (*__MALLOC_HOOK_VOLATILE __realloc_hook)(void*, size_t, const void*) =
    &glibc_override_realloc;
void (*__MALLOC_HOOK_VOLATILE __free_hook)(void*,
                                           const void*) = &glibc_override_free;
void* (*__MALLOC_HOOK_VOLATILE __memalign_hook)(size_t, size_t, const void*) =
    &glibc_override_memalign;

}  // extern "C"

inline constexpr size_t size_for_cpp_new(size_t size) {
  // The C++ spec requires new() operators that throw exceptions to always
  // return a distint non-null pointer.
  //
  // To facilitate this while using `malloc`, which may return `NULL` for
  // zero-sized allocations, we just request a small size.
  return size == 0 ? 1 : size;
}

void* operator new(size_t size) noexcept(false) {
  void* res = bench::malloc(size_for_cpp_new(size));
  if (res == nullptr) {
    throw std::bad_alloc();
  }
  return res;
}
void operator delete(void* p) noexcept {
  bench::free(p);
}
void operator delete(void* p, size_t size) noexcept {
  bench::free(p, size_for_cpp_new(size));
}
void* operator new[](size_t size) noexcept(false) {
  void* res = bench::malloc(size_for_cpp_new(size));
  if (res == nullptr) {
    throw std::bad_alloc();
  }
  return res;
}
void operator delete[](void* p) noexcept {
  bench::free(p);
}
void operator delete[](void* p, size_t size) noexcept {
  bench::free(p, size_for_cpp_new(size));
}
void* operator new(size_t size, const std::nothrow_t&) noexcept {
  return bench::malloc(size_for_cpp_new(size));
}
void* operator new[](size_t size, const std::nothrow_t&) noexcept {
  return bench::malloc(size_for_cpp_new(size));
}
void operator delete(void* p, const std::nothrow_t&) noexcept {
  bench::free(p);
}
void operator delete[](void* p, const std::nothrow_t&) noexcept {
  bench::free(p);
}

void* operator new(size_t size, std::align_val_t alignment) noexcept(false) {
  void* res =
      bench::malloc(size_for_cpp_new(size), static_cast<size_t>(alignment));
  if (res == nullptr) {
    throw std::bad_alloc();
  }
  return res;
}
void* operator new(size_t size, std::align_val_t alignment,
                   const std::nothrow_t&) noexcept {
  return bench::malloc(size_for_cpp_new(size), static_cast<size_t>(alignment));
}
void operator delete(void* p, std::align_val_t alignment) noexcept {
  bench::free(p, size_for_cpp_new(0), static_cast<size_t>(alignment));
}
void operator delete(void* p, std::align_val_t alignment,
                     const std::nothrow_t&) noexcept {
  bench::free(p, size_for_cpp_new(0), static_cast<size_t>(alignment));
}
void operator delete(void* p, size_t size,
                     std::align_val_t alignment) noexcept {
  bench::free(p, size_for_cpp_new(size), static_cast<size_t>(alignment));
}
void* operator new[](size_t size, std::align_val_t alignment) noexcept(false) {
  void* res =
      bench::malloc(size_for_cpp_new(size), static_cast<size_t>(alignment));
  if (res == nullptr) {
    throw std::bad_alloc();
  }
  return res;
}
void* operator new[](size_t size, std::align_val_t alignment,
                     const std::nothrow_t&) noexcept {
  return bench::malloc(size_for_cpp_new(size), static_cast<size_t>(alignment));
}
void operator delete[](void* p, std::align_val_t alignment) noexcept {
  bench::free(p, size_for_cpp_new(0), static_cast<size_t>(alignment));
}
void operator delete[](void* p, std::align_val_t alignment,
                       const std::nothrow_t&) noexcept {
  bench::free(p, size_for_cpp_new(0), static_cast<size_t>(alignment));
}
void operator delete[](void* p, size_t size,
                       std::align_val_t alignment) noexcept {
  bench::free(p, size_for_cpp_new(size), static_cast<size_t>(alignment));
}

extern "C" {

void* malloc(size_t size) noexcept {
  return bench::malloc(size);
}
void free(void* ptr) noexcept {
  bench::free(ptr);
}
void free_sized(void* ptr, size_t size) noexcept {
  bench::free(ptr, size);
}
void free_aligned_sized(void* ptr, size_t align, size_t size) noexcept {
  bench::free(ptr, size, align);
}
void* realloc(void* ptr, size_t size) noexcept {
  return bench::realloc(ptr, size);
}
void* reallocarray(void* ptr, size_t nmemb, size_t size) noexcept {
  return bench::realloc(ptr, nmemb * size);
}
void* calloc(size_t nmemb, size_t size) noexcept {
  return bench::calloc(nmemb, size);
}
void cfree(void* ptr) noexcept {
  bench::free(ptr);
}
void* memalign(size_t __alignment, size_t __size) noexcept {
  return bench::malloc(__size, __alignment);
}
void* aligned_alloc(size_t __alignment, size_t __size) noexcept {
  return bench::malloc(__size, __alignment);
}
void* valloc(size_t size) noexcept {
  return bench::malloc(size, 4096);
}
void* pvalloc(size_t size) noexcept {
  return bench::malloc((size + 4095) & ~4096, 4096);
}
int posix_memalign(void** __memptr, size_t __alignment,
                   size_t __size) noexcept {
  return posix_memalign_helper(__memptr, __alignment, __size);
}
void malloc_stats(void) noexcept {}
int malloc_trim(size_t pad) noexcept {
  (void) pad;
  return 0;
}
int mallopt(int __param, int __value) noexcept {
  (void) __param;
  (void) __value;
  return 1;
}
int malloc_info(int __options, FILE* __fp) noexcept {
  (void) __options;
  fputs("<malloc></malloc>\n", __fp);
  return 0;
}
size_t malloc_size(void* p) noexcept {
  return bench::get_size(p);
}
size_t malloc_usable_size(void* __ptr) noexcept {
  return bench::get_size(__ptr);
}

}  // extern "C"

// NOLINTEND(bugprone-reserved-identifier, readability-identifier-naming)

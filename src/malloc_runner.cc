#include "src/malloc_runner.h"

#include "util/absl_util.h"

#include "src/allocator_interface.h"
#include "src/heap_factory.h"

namespace bench {

MallocRunner::MallocRunner(HeapFactory& heap_factory, bool verbose)
    : heap_factory_(&heap_factory), verbose_(verbose) {}

absl::Status MallocRunner::InitializeHeap() {
  heap_factory_->Reset();
  initialize_heap(*heap_factory_);
  return absl::OkStatus();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
absl::Status MallocRunner::CleanupHeap() {
  return absl::OkStatus();
}

absl::StatusOr<void*> MallocRunner::Malloc(size_t size,
                                           std::optional<size_t> alignment) {
  if (verbose_) {
    if (alignment.has_value()) {
      std::cout << "aligned_alloc(" << size << ", " << alignment.value() << ")"
                << std::endl;
    } else {
      std::cout << "malloc(" << size << ")" << std::endl;
    }
  }

  void* ptr = bench::malloc(size, alignment.value_or(0));

  if (size == 0 && ptr != nullptr) {
    return absl::InternalError(
        absl::StrFormat("%s Expected `nullptr` return value on malloc with "
                        "size 0: %p = malloc(%zu)",
                        kFailedTestPrefix, ptr, size));
  }

  RETURN_IF_ERROR(PostAlloc(ptr, size, alignment, /*is_calloc=*/false));
  return ptr;
}

absl::StatusOr<void*> MallocRunner::Calloc(size_t nmemb, size_t size) {
  if (verbose_) {
    std::cout << "calloc(" << nmemb << ", " << size << ")" << std::endl;
  }

  void* ptr = bench::calloc(nmemb, size);

  if ((nmemb == 0 || size == 0) && ptr != nullptr) {
    return absl::InternalError(
        absl::StrFormat("%s Expected `nullptr` return value on calloc with "
                        "size 0: %p = calloc(%zu, %zu)",
                        kFailedTestPrefix, ptr, nmemb, size));
  }

  RETURN_IF_ERROR(PostAlloc(ptr, nmemb * size,
                            /*alignment=*/std::nullopt,
                            /*is_calloc=*/true));
  return ptr;
}

absl::StatusOr<void*> MallocRunner::Realloc(void* ptr, size_t size) {
  if (verbose_) {
    std::cout << "realloc(" << ptr << ", " << size << ")" << std::endl;
  }

  if (ptr == nullptr) {
    void* new_ptr = bench::realloc(nullptr, size);
    RETURN_IF_ERROR(PostAlloc(new_ptr, size, /*alignment=*/std::nullopt,
                              /*is_calloc=*/false));
    return new_ptr;
  }

  RETURN_IF_ERROR(PreRealloc(ptr, size));
  void* new_ptr = bench::realloc(ptr, size);
  RETURN_IF_ERROR(PostRealloc(new_ptr, /*old_ptr=*/ptr, size));
  return new_ptr;
}

absl::Status MallocRunner::Free(void* ptr, std::optional<size_t> size_hint,
                                std::optional<size_t> alignment_hint) {
  RETURN_IF_ERROR(PreRelease(ptr));
  bench::free(ptr, size_hint.value_or(0), alignment_hint.value_or(0));
  return absl::OkStatus();
}

}  // namespace bench

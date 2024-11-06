#pragma once

#include <cstddef>
#include <optional>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "util/absl_util.h"

#include "src/allocator_interface.h"
#include "src/heap_factory.h"
#include "src/tracefile_executor.h"  // IWYU pragma: keep

namespace bench {

struct MallocRunnerOptions {
  bool verbose = false;
};

struct MallocRunnerConfig {
  bool perftest = false;
};

template <typename ReallocData,
          MallocRunnerConfig Config = MallocRunnerConfig()>
class MallocRunner {
 public:
  static constexpr char kFailedTestPrefix[] = "[Failed]";

  explicit MallocRunner(
      const MallocRunnerOptions& options = MallocRunnerOptions());
  explicit MallocRunner(
      HeapFactory& heap_factory,
      const MallocRunnerOptions& options = MallocRunnerOptions());

  static bool IsFailedTestStatus(const absl::Status& status);

  virtual absl::Status PostAlloc(void* ptr, size_t size,
                                 std::optional<size_t> alignment,
                                 bool is_calloc) = 0;

  virtual absl::StatusOr<ReallocData> PreRealloc(void* ptr, size_t size) = 0;

  virtual absl::Status PostRealloc(void* new_ptr, void* old_ptr, size_t size,
                                   ReallocData realloc_data) = 0;

  virtual absl::Status PreRelease(void* ptr) = 0;

  absl::Status InitializeHeap();
  absl::Status CleanupHeap();
  absl::StatusOr<void*> Malloc(size_t size, std::optional<size_t> alignment);
  absl::StatusOr<void*> Calloc(size_t nmemb, size_t size);
  absl::StatusOr<void*> Realloc(void* ptr, size_t size);
  absl::Status Free(void* ptr, std::optional<size_t> size_hint,
                    std::optional<size_t> alignment_hint);

  HeapFactory& HeapFactoryRef() {
    return *heap_factory_;
  }
  const HeapFactory& HeapFactoryRef() const {
    return *heap_factory_;
  }

 private:
  HeapFactory* heap_factory_;

  const MallocRunnerOptions options_;
};

static_assert(TracefileAllocator<MallocRunner<bool, MallocRunnerConfig{}>>);

template <typename ReallocData, MallocRunnerConfig Config>
MallocRunner<ReallocData, Config>::MallocRunner(
    const MallocRunnerOptions& options)
    : heap_factory_(nullptr), options_(options) {}

template <typename ReallocData, MallocRunnerConfig Config>
MallocRunner<ReallocData, Config>::MallocRunner(
    HeapFactory& heap_factory, const MallocRunnerOptions& options)
    : heap_factory_(&heap_factory), options_(options) {}

/* static */
template <typename ReallocData, MallocRunnerConfig Config>
bool MallocRunner<ReallocData, Config>::IsFailedTestStatus(
    const absl::Status& status) {
  return status.message().starts_with(kFailedTestPrefix);
}

template <typename ReallocData, MallocRunnerConfig Config>
absl::Status MallocRunner<ReallocData, Config>::InitializeHeap() {
  heap_factory_->Reset();
  bench::initialize_heap(*heap_factory_);
  return absl::OkStatus();
}

template <typename ReallocData, MallocRunnerConfig Config>
absl::Status MallocRunner<ReallocData, Config>::CleanupHeap() {
  return absl::OkStatus();
}

template <typename ReallocData, MallocRunnerConfig Config>
absl::StatusOr<void*> MallocRunner<ReallocData, Config>::Malloc(
    size_t size, std::optional<size_t> alignment) {
  if constexpr (Config.perftest) {
    return bench::malloc(size, alignment.value_or(0));
  }

  if (options_.verbose) {
    if (alignment.has_value()) {
      std::cout << "aligned_alloc(" << size << ", " << alignment.value() << ")"
                << std::flush;
    } else {
      std::cout << "malloc(" << size << ")" << std::flush;
    }
  }

  void* ptr = bench::malloc(size, alignment.value_or(0));

  if (options_.verbose) {
    std::cout << " = " << ptr << std::endl;
  }

  if (size == 0 && ptr != nullptr) {
    return absl::InternalError(
        absl::StrFormat("%s Expected `nullptr` return value on malloc with "
                        "size 0: %p = malloc(%zu)",
                        kFailedTestPrefix, ptr, size));
  }

  RETURN_IF_ERROR(PostAlloc(ptr, size, alignment, /*is_calloc=*/false));
  return ptr;
}

template <typename ReallocData, MallocRunnerConfig Config>
absl::StatusOr<void*> MallocRunner<ReallocData, Config>::Calloc(size_t nmemb,
                                                                size_t size) {
  if constexpr (Config.perftest) {
    return bench::malloc(nmemb * size);
  }

  if (options_.verbose) {
    std::cout << "calloc(" << nmemb << ", " << size << ")" << std::flush;
  }

  void* ptr = bench::calloc(nmemb, size);

  if (options_.verbose) {
    std::cout << " = " << ptr << std::endl;
  }

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

template <typename ReallocData, MallocRunnerConfig Config>
absl::StatusOr<void*> MallocRunner<ReallocData, Config>::Realloc(void* ptr,
                                                                 size_t size) {
  if constexpr (Config.perftest) {
    return bench::realloc(ptr, size);
  }

  if (options_.verbose) {
    std::cout << "realloc(" << ptr << ", " << size << ")" << std::flush;
  }

  if (ptr == nullptr) {
    void* new_ptr = bench::realloc(nullptr, size);

    if (options_.verbose) {
      std::cout << " = " << ptr << std::endl;
    }

    RETURN_IF_ERROR(PostAlloc(new_ptr, size, /*alignment=*/std::nullopt,
                              /*is_calloc=*/false));
    return new_ptr;
  }

  DEFINE_OR_RETURN(ReallocData, realloc_data, PreRealloc(ptr, size));
  void* new_ptr = bench::realloc(ptr, size);

  if (options_.verbose) {
    std::cout << " = " << ptr << std::endl;
  }

  RETURN_IF_ERROR(
      PostRealloc(new_ptr, /*old_ptr=*/ptr, size, std::move(realloc_data)));
  return new_ptr;
}

template <typename ReallocData, MallocRunnerConfig Config>
absl::Status MallocRunner<ReallocData, Config>::Free(
    void* ptr, std::optional<size_t> size_hint,
    std::optional<size_t> alignment_hint) {
  if (options_.verbose) {
    std::cout << "free(" << ptr << ")" << std::endl;
  }

  if constexpr (!Config.perftest) {
    RETURN_IF_ERROR(PreRelease(ptr));
  }
  bench::free(ptr, size_hint.value_or(0), alignment_hint.value_or(0));
  return absl::OkStatus();
}

}  // namespace bench

#include "src/utiltest.h"

#include <atomic>

#include "absl/flags/flag.h"
#include "absl/status/statusor.h"
#include "util/absl_util.h"

#include "src/heap_factory.h"
#include "src/malloc_runner.h"
#include "src/tracefile_executor.h"
#include "src/tracefile_reader.h"

ABSL_FLAG(bool, effective_util, false,
          "If set, uses a \"more fair\" measure of memory utilization, "
          "rounding up each allocation size to its alignment requirement.");

namespace bench {

namespace {

size_t RoundUp(size_t size) {
  if (!absl::GetFlag(FLAGS_effective_util)) {
    return size;
  }

  if (size <= 8) {
    return 8;
  }
  return (size + 0xf) & ~0xf;
}

}  // namespace

/* static */
absl::StatusOr<double> Utiltest::MeasureUtilization(
    TracefileReader& reader, HeapFactory& heap_factory,
    const TracefileExecutorOptions& options) {
  Utiltest utiltest(reader, heap_factory);
  RETURN_IF_ERROR(utiltest.Run(options));
  return utiltest.ComputeUtilization();
}

Utiltest::Utiltest(TracefileReader& reader, HeapFactory& heap_factory)
    : MallocRunner(reader, heap_factory) {}

absl::Status Utiltest::PostAlloc(void* ptr, size_t size,
                                 std::optional<size_t> alignment,
                                 bool is_calloc) {
  (void) alignment;
  (void) is_calloc;

  size_t rounded_size = RoundUp(size);
  size_t total_allocated_bytes = total_allocated_bytes_.fetch_add(
                                     rounded_size, std::memory_order_relaxed) +
                                 rounded_size;
  RecomputeMax(total_allocated_bytes);

  auto [it, inserted] = size_map_.insert({ ptr, size });
  if (!inserted) {
    return absl::InternalError(
        absl::StrFormat("Allocated pointer %p of size %zu conflicts with "
                        "existing allocation %p of size %zu",
                        ptr, size, it->first, it->second));
  }

  return absl::OkStatus();
}

absl::Status Utiltest::PreRealloc(void* ptr, size_t size) {
  (void) ptr;
  (void) size;
  return absl::OkStatus();
}

absl::Status Utiltest::PostRealloc(void* new_ptr, void* old_ptr, size_t size) {
  auto it = size_map_.find(old_ptr);
  if (it == size_map_.end()) {
    return absl::InternalError(absl::StrFormat(
        "Reallocated memory %p not found in size map.", old_ptr));
  }
  const size_t old_size = RoundUp(it->second);

  size_t rounded_size = RoundUp(size);
  size_t total_allocated_bytes =
      total_allocated_bytes_.fetch_add(rounded_size - old_size,
                                       std::memory_order_relaxed) +
      (rounded_size - old_size);
  RecomputeMax(total_allocated_bytes);

  if (new_ptr == old_ptr) {
    auto result = size_map_.assign(new_ptr, size);
    if (!result.has_value()) {
      return absl::InternalError(
          absl::StrFormat("Reassigning size of realloc-ed memory %p from %zu "
                          "to %zu failed, not found in map.",
                          new_ptr, old_size, size));
    }
    return absl::OkStatus();
  }

  size_t deleted_elems = size_map_.erase(old_ptr);
  if (deleted_elems != 1) {
    return absl::InternalError(absl::StrFormat(
        "Erasing old realloc-ed memory %p failed. This indicates operations on "
        "a pointer were not properly serialized.",
        old_ptr));
  }

  auto [new_it, inserted] = size_map_.insert({ new_ptr, size });
  if (!inserted) {
    return absl::InternalError(
        absl::StrFormat("Reallocated pointer %p of size %zu conflicts with "
                        "existing allocation %p of size %zu",
                        new_ptr, size, new_it->first, new_it->second));
  }

  return absl::OkStatus();
}

absl::Status Utiltest::PreRelease(void* ptr) {
  if (ptr == nullptr) {
    return absl::OkStatus();
  }

  auto it = size_map_.find(ptr);
  if (it == size_map_.end()) {
    return absl::InternalError(
        absl::StrFormat("Freed memory %p not found in size map.", ptr));
  }
  const size_t old_size = RoundUp(it->second);

  size_t total_allocated_bytes =
      total_allocated_bytes_.fetch_sub(old_size, std::memory_order_relaxed) -
      old_size;
  // Recompute max here in case heap size changed (possible in theory).
  RecomputeMax(total_allocated_bytes);

  size_t deleted_elems = size_map_.erase(ptr);
  if (deleted_elems != 1) {
    return absl::InternalError(
        absl::StrFormat("Erasing old freed memory %p failed. This indicates "
                        "operations on a pointer were not properly serialized.",
                        ptr));
  }

  return absl::OkStatus();
}

void Utiltest::RecomputeMax(size_t total_allocated_bytes) {
  size_t heap_size = 0;
  HeapFactoryRef().WithInstances<void>([&heap_size](const auto& instances) {
    for (const auto& heap : instances) {
      heap_size += heap->Size();
    }
  });

  // Update the max total allocated bytes and total heap size:
  size_t prev_max;
  while ((prev_max = max_allocated_bytes_.exchange(total_allocated_bytes,
                                                   std::memory_order_relaxed)) >
         total_allocated_bytes) {
    total_allocated_bytes = prev_max;
  }
  while ((prev_max = max_heap_size_.exchange(
              heap_size, std::memory_order_release)) > heap_size) {
    heap_size = prev_max;
  }
}

absl::StatusOr<double> Utiltest::ComputeUtilization() const {
  if (total_allocated_bytes_.load(std::memory_order_relaxed) != 0) {
    return absl::InternalError(
        "Tracefile does not free all the memory it allocates.");
  }

  size_t max_heap_size = max_heap_size_.load(std::memory_order_acquire);
  size_t max_allocated_bytes =
      max_allocated_bytes_.load(std::memory_order_relaxed);
  return max_heap_size != 0
             ? static_cast<double>(max_allocated_bytes) / max_heap_size
             : -1;
}

}  // namespace bench

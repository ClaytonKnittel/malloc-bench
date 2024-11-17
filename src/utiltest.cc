#include "src/utiltest.h"

#include <atomic>
#include <cstddef>

#include "absl/flags/flag.h"
#include "absl/status/status.h"
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

Utiltest::Utiltest(HeapFactory& heap_factory) : MallocRunner(heap_factory) {}

/* static */
absl::StatusOr<double> Utiltest::MeasureUtilization(
    TracefileReader& reader, HeapFactory& heap_factory,
    const TracefileExecutorOptions& options) {
  TracefileExecutor<Utiltest> utiltest(reader, std::ref(heap_factory));
  RETURN_IF_ERROR(utiltest.Run(options).status());
  return utiltest.Inner().ComputeUtilization();
}

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
        absl::StrFormat("%s Allocated pointer %p of size %zu conflicts with "
                        "existing allocation %p of size %zu",
                        kFailedTestPrefix, ptr, size, it->first, it->second));
  }

  return absl::OkStatus();
}

absl::StatusOr<size_t> Utiltest::PreRealloc(void* ptr, size_t size) {
  (void) size;
  auto it = size_map_.find(ptr);
  if (it == size_map_.end()) {
    return absl::InternalError(
        absl::StrFormat("%s Reallocated memory %p not found in size map.",
                        kFailedTestPrefix, ptr));
  }
  size_t prev_size = it->second;

  size_t deleted_elems = size_map_.erase(ptr);
  if (deleted_elems != 1) {
    return absl::InternalError(absl::StrFormat(
        "Erasing old realloc-ed memory %p failed. This indicates operations on "
        "a pointer were not properly serialized.",
        ptr));
  }

  return RoundUp(prev_size);
}

absl::Status Utiltest::PostRealloc(void* new_ptr, void* old_ptr, size_t size,
                                   size_t prev_size) {
  (void) old_ptr;

  size_t rounded_size = RoundUp(size);
  size_t total_allocated_bytes =
      total_allocated_bytes_.fetch_add(rounded_size - prev_size,
                                       std::memory_order_relaxed) +
      (rounded_size - prev_size);
  RecomputeMax(total_allocated_bytes);

  auto [new_it, inserted] = size_map_.insert({ new_ptr, size });
  if (!inserted) {
    return absl::InternalError(absl::StrFormat(
        "%s Reallocated pointer %p of size %zu conflicts with "
        "existing allocation %p of size %zu",
        kFailedTestPrefix, new_ptr, size, new_it->first, new_it->second));
  }

  return absl::OkStatus();
}

absl::Status Utiltest::PreRelease(void* ptr) {
  if (ptr == nullptr) {
    return absl::OkStatus();
  }

  auto it = size_map_.find(ptr);
  if (it == size_map_.end()) {
    return absl::InternalError(absl::StrFormat(
        "%s Freed memory %p not found in size map.", kFailedTestPrefix, ptr));
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

#include <cstddef>
#include <cstdlib>

#include "absl/status/statusor.h"
#include "absl/time/time.h"

#include "src/allocator_interface.h"
#include "src/heap_factory.h"
#include "src/tracefile_reader.h"

namespace bench {

using proto::TraceLine;

// Runs at least 1000000 ops, and returns the average MOps/s.
absl::StatusOr<double> TimeTrace(TracefileReader& reader,
                                 HeapFactory& heap_factory,
                                 size_t min_desired_ops) {
  size_t num_repetitions = (min_desired_ops - 1) / reader.size() + 1;
  std::vector<void*> ptrs(reader.Tracefile().max_simultaneous_allocs());

  heap_factory.Reset();

  absl::Time start = absl::Now();
  initialize_heap(heap_factory);
  for (size_t t = 0; t < num_repetitions; t++) {
    for (const TraceLine& line : reader) {
      switch (line.op_case()) {
        case TraceLine::kMalloc: {
          void* ptr = malloc(line.malloc().input_size(),
                             line.malloc().has_input_alignment()
                                 ? line.malloc().input_alignment()
                                 : 0);
          ptrs[line.malloc().result_id()] = ptr;
          break;
        }
        case TraceLine::kCalloc: {
          void* ptr =
              malloc(line.calloc().input_nmemb() * line.calloc().input_size());
          ptrs[line.calloc().result_id()] = ptr;
          break;
        }
        case TraceLine::kRealloc: {
          void* ptr = realloc(line.realloc().has_input_id()
                                  ? ptrs[line.realloc().input_id()]
                                  : nullptr,
                              line.realloc().input_size());
          ptrs[line.realloc().result_id()] = ptr;
          break;
        }
        case TraceLine::kFree: {
          free(line.free().has_input_id() ? ptrs[line.free().input_id()]
                                          : nullptr,
               line.free().has_input_size_hint() ? line.free().input_size_hint()
                                                 : 0,
               line.free().has_input_alignment_hint()
                   ? line.free().input_alignment_hint()
                   : 0);
          break;
        }
        case TraceLine::OP_NOT_SET: {
          __builtin_unreachable();
        }
      }
    }
  }
  absl::Time end = absl::Now();

  size_t total_ops = num_repetitions * reader.size();
  double seconds = absl::FDivDuration((end - start), absl::Seconds(1));
  return total_ops / seconds / 1000000;
}

}  // namespace bench

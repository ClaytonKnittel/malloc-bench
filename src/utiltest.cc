#include <optional>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "util/absl_util.h"

#include "src/allocator_interface.h"
#include "src/singleton_heap.h"
#include "src/tracefile_reader.h"

namespace bench {

size_t RoundUp(size_t size) {
	return size;
  if (size <= 8) {
    return 8;
  }
  return (size + 0xf) & ~0xf;
}

absl::StatusOr<double> MeasureUtilization(const std::string& tracefile) {
  absl::flat_hash_map<void*, std::pair<void*, size_t>> id_to_ptrs;
  DEFINE_OR_RETURN(TracefileReader, reader, TracefileReader::Open(tracefile));

  SingletonHeap::GlobalInstance()->Reset();
  initialize_heap();

  size_t total_allocated_bytes = 0;
  size_t max_allocated_bytes = 0;
  while (true) {
    DEFINE_OR_RETURN(std::optional<TraceLine>, line, reader.NextLine());
    if (!line.has_value()) {
      break;
    }

    switch (line->op) {
      case TraceLine::Op::kMalloc: {
        void* ptr = malloc(line->input_size);
        if (ptr != nullptr) {
          id_to_ptrs[line->result] = { ptr, line->input_size };
          total_allocated_bytes += RoundUp(line->input_size);
        }
        break;
      }
      case TraceLine::Op::kCalloc: {
        void* ptr = calloc(line->nmemb, line->input_size);
        if (ptr != nullptr) {
          id_to_ptrs[line->result] = { ptr, line->input_size };
          total_allocated_bytes += RoundUp(line->input_size);
        }
        break;
      }
      case TraceLine::Op::kRealloc: {
        void* new_ptr =
            realloc(id_to_ptrs[line->input_ptr].first, line->input_size);
        if (line->input_ptr != nullptr) {
          total_allocated_bytes -= RoundUp(id_to_ptrs[line->input_ptr].second);
          id_to_ptrs.erase(line->input_ptr);
        }
        total_allocated_bytes += RoundUp(line->input_size);
        id_to_ptrs[line->result] = { new_ptr, line->input_size };
        break;
      }
      case TraceLine::Op::kFree: {
        if (line->input_ptr == nullptr) {
          free(nullptr);
          break;
        }

        free(id_to_ptrs[line->input_ptr].first);
        total_allocated_bytes -= RoundUp(id_to_ptrs[line->input_ptr].second);
        id_to_ptrs.erase(line->input_ptr);
        break;
      }
    }

    max_allocated_bytes = std::max(total_allocated_bytes, max_allocated_bytes);
  }

  if (total_allocated_bytes != 0) {
    for (const auto& [id, ptr] : id_to_ptrs) {
      printf("%p: %p %zu\n", id, ptr.first, ptr.second);
    }
    return absl::InternalError(
        "Tracefile does not free all the memory it allocates.");
  }

  return static_cast<double>(max_allocated_bytes) /
         SingletonHeap::GlobalInstance()->Size();
}

}  // namespace bench

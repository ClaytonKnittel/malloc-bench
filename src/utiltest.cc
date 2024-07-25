#include <optional>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"

#include "src/allocator_interface.h"
#include "src/fake_heap.h"
#include "src/tracefile_reader.h"
#include "src/util.h"

namespace bench {

absl::StatusOr<double> MeasureUtilization(const std::string& tracefile) {
  absl::flat_hash_map<void*, std::pair<void*, size_t>> id_to_ptrs;
  DEFINE_OR_RETURN(TracefileReader, reader, TracefileReader::Open(tracefile));

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
        id_to_ptrs[line->result] = { ptr, line->input_size };
        total_allocated_bytes += line->input_size;
        break;
      }
      case TraceLine::Op::kCalloc: {
        void* ptr = calloc(line->nmemb, line->input_size);
        id_to_ptrs[line->result] = { ptr, line->input_size };
        total_allocated_bytes += line->input_size;
        break;
      }
      case TraceLine::Op::kRealloc: {
        void* new_ptr =
            realloc(id_to_ptrs[line->input_ptr].first, line->input_size);
        total_allocated_bytes -= id_to_ptrs[line->input_ptr].second;
        total_allocated_bytes += line->input_size;
        id_to_ptrs.erase(line->input_ptr);
        id_to_ptrs[line->result] = { new_ptr, line->input_size };
        break;
      }
      case TraceLine::Op::kFree: {
        free(id_to_ptrs[line->input_ptr].first);
        total_allocated_bytes -= id_to_ptrs[line->input_ptr].second;
        id_to_ptrs.erase(line->input_ptr);
        break;
      }
    }

    max_allocated_bytes = std::max(total_allocated_bytes, max_allocated_bytes);
  }

  return static_cast<double>(max_allocated_bytes) /
         FakeHeap::GlobalInstance()->Size();
}

}  // namespace bench

int main() {
  auto status = bench::MeasureUtilization("traces/simple_realloc.trace");
  if (!status.ok()) {
    std::cerr << status.status() << std::endl;
    return -1;
  }

  std::cout << "Completed trace, utilization: " << status.value() << std::endl;
  return 0;
}

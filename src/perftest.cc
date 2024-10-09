<<<<<<< HEAD
#include <cstddef>
#include <cstdlib>

#include "absl/status/statusor.h"
#include "absl/time/time.h"

#include "src/allocator_interface.h"
#include "src/heap_factory.h"
=======

#include <cstdlib>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "util/absl_util.h"

#include "src/allocator_interface.h"
#include "src/singleton_heap.h"
>>>>>>> d3b973fd6e938786ae4ec0560b204de2d3ba8e58
#include "src/tracefile_reader.h"

namespace bench {

<<<<<<< HEAD
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
          void* ptr = malloc(line.malloc().input_size());
          ptrs[line.malloc().result_id()] = ptr;
          break;
        }
        case TraceLine::kCalloc: {
          void* ptr =
              calloc(line.calloc().input_nmemb(), line.calloc().input_size());
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
                                          : nullptr);
          break;
        }
        case TraceLine::OP_NOT_SET: {
          __builtin_unreachable();
        }
=======
struct TimeOp {
  TraceLine line;
  size_t arg_idx;
  size_t res_idx;
};

namespace {

std::pair<std::vector<TimeOp>, size_t> ComputeOps(
    const std::vector<TraceLine>& lines) {
  absl::flat_hash_map<void*, size_t> idx_map;
  std::vector<size_t> free_idxs;
  size_t next_idx = 0;

  auto get_new_idx = [&free_idxs, &next_idx]() {
    if (free_idxs.empty()) {
      return next_idx++;
    }

    size_t res = free_idxs.back();
    free_idxs.pop_back();
    return res;
  };

  auto free_idx = [&free_idxs](size_t idx) { free_idxs.push_back(idx); };

  std::vector<TimeOp> ops;
  ops.reserve(lines.size());
  for (const TraceLine& line : lines) {
    switch (line.op) {
      case TraceLine::Op::kMalloc: {
        size_t idx = get_new_idx();
        ops.push_back(TimeOp{
            .line = line,
            .res_idx = idx,
        });
        idx_map[line.result] = idx;
        break;
      }
      case TraceLine::Op::kCalloc: {
        size_t idx = get_new_idx();
        ops.push_back(TimeOp{
            .line = line,
            .res_idx = idx,
        });
        idx_map[line.result] = idx;
        break;
      }
      case TraceLine::Op::kRealloc: {
        if (line.input_ptr != line.result) {
          size_t old_idx = idx_map[line.input_ptr];
          size_t idx = get_new_idx();
          ops.push_back(TimeOp{
              .line = line,
              .arg_idx = old_idx,
              .res_idx = idx,
          });
          idx_map[line.result] = idx;
          idx_map.erase(line.input_ptr);
          free_idx(old_idx);
        } else {
          ops.push_back(TimeOp{
              .line = line,
              .arg_idx = idx_map[line.input_ptr],
              .res_idx = idx_map[line.input_ptr],
          });
        }
        break;
      }
      case TraceLine::Op::kFree: {
        if (line.input_ptr == nullptr) {
          break;
        }
        size_t idx = idx_map[line.input_ptr];
        ops.push_back(TimeOp{
            .line = line,
            .arg_idx = idx,
        });
        idx_map.erase(line.input_ptr);
        free_idx(idx);
        break;
      }
    }
  }

  return { ops, next_idx };
}

}  // namespace

// Runs at least 1000000 ops, and returns the average MOps/s.
absl::StatusOr<double> TimeTrace(const std::string& tracefile) {
  constexpr size_t kMinDesiredOps = 1000000;

  DEFINE_OR_RETURN(TracefileReader, reader, TracefileReader::Open(tracefile));

  DEFINE_OR_RETURN(std::vector<TraceLine>, lines, reader.CollectLines());

  size_t num_repetitions = (kMinDesiredOps - 1) / lines.size() + 1;

  auto [ops, max_allocs] = ComputeOps(lines);
  std::vector<void*> ptrs(max_allocs);

  absl::Time start = absl::Now();
  for (size_t t = 0; t < num_repetitions; t++) {
    SingletonHeap::GlobalInstance()->Reset();
    initialize_heap();

    for (const TimeOp& op : ops) {
      switch (op.line.op) {
        case TraceLine::Op::kMalloc: {
          void* ptr = malloc(op.line.input_size);
          ptrs[op.res_idx] = ptr;
          break;
        }
        case TraceLine::Op::kCalloc: {
          void* ptr = calloc(op.line.nmemb, op.line.input_size);
          ptrs[op.res_idx] = ptr;
          break;
        }
        case TraceLine::Op::kRealloc: {
          void* ptr = realloc(ptrs[op.arg_idx], op.line.input_size);
          ptrs[op.res_idx] = ptr;
          break;
        }
        case TraceLine::Op::kFree: {
          free(ptrs[op.arg_idx]);
          break;
        }
>>>>>>> d3b973fd6e938786ae4ec0560b204de2d3ba8e58
      }
    }
  }
  absl::Time end = absl::Now();

<<<<<<< HEAD
  size_t total_ops = num_repetitions * reader.size();
=======
  size_t total_ops = num_repetitions * ops.size();
>>>>>>> d3b973fd6e938786ae4ec0560b204de2d3ba8e58
  double seconds = absl::FDivDuration((end - start), absl::Seconds(1));
  return total_ops / seconds / 1000000;
}

}  // namespace bench

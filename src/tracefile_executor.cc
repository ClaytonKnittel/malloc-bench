#include "src/tracefile_executor.h"

#include "absl/status/status.h"
#include "util/absl_util.h"

#include "proto/tracefile.pb.h"
#include "src/heap_factory.h"
#include "src/tracefile_reader.h"

namespace bench {

using proto::TraceLine;

TracefileExecutor::TracefileExecutor(TracefileReader&& reader,
                                     HeapFactory& heap_factory)
    : reader_(std::move(reader)), heap_factory_(&heap_factory) {}

absl::Status TracefileExecutor::Run() {
  InitializeHeap(*heap_factory_);
  return ProcessTracefile();
}

absl::Status TracefileExecutor::ProcessTracefile() {
  // A map from allocation id's to pointers returned from the allocator. Since
  // id's are assigned contiguously from lowest to highest ID, they can be
  // stored in a vector.
  std::vector<void*> id_map(reader_.Tracefile().max_simultaneous_allocs());

  for (const TraceLine& line : reader_) {
    switch (line.op_case()) {
      case TraceLine::kMalloc: {
        const TraceLine::Malloc& malloc = line.malloc();
        DEFINE_OR_RETURN(void*, ptr, Malloc(malloc.input_size()));

        if (malloc.input_size() != 0 && malloc.has_result_id()) {
          id_map[malloc.result_id()] = ptr;
        }
        break;
      }
      case TraceLine::kCalloc: {
        const TraceLine::Calloc& calloc = line.calloc();
        DEFINE_OR_RETURN(void*, ptr,
                         Calloc(calloc.input_nmemb(), calloc.input_size()));

        if (calloc.input_nmemb() != 0 && calloc.input_size() != 0 &&
            calloc.has_result_id()) {
          id_map[calloc.result_id()] = ptr;
        }
        break;
      }
      case TraceLine::kRealloc: {
        const TraceLine::Realloc& realloc = line.realloc();
        void* input_ptr;
        if (realloc.has_input_id()) {
          input_ptr = id_map[realloc.input_id()];
        } else {
          input_ptr = nullptr;
        }
        DEFINE_OR_RETURN(void*, result_ptr,
                         Realloc(input_ptr, realloc.input_size()));

        id_map[realloc.result_id()] = result_ptr;
        break;
      }
      case TraceLine::kFree: {
        const TraceLine::Free& free = line.free();
        if (!free.has_input_id()) {
          RETURN_IF_ERROR(Free(nullptr));
          break;
        }

        void* ptr = id_map[free.input_id()];
        RETURN_IF_ERROR(Free(ptr));
        break;
      }
      case TraceLine::OP_NOT_SET: {
        return absl::FailedPreconditionError("Op not set in tracefile");
      }
    }
  }

  return absl::OkStatus();
}

}  // namespace bench

#include "src/tracefile_executor.h"

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "util/absl_util.h"

#include "src/heap_factory.h"
#include "src/tracefile_reader.h"

namespace bench {

TracefileExecutor::TracefileExecutor(TracefileReader&& reader,
                                     HeapFactory& heap_factory)
    : reader_(std::move(reader)), heap_factory_(&heap_factory) {}

absl::Status TracefileExecutor::Run() {
  InitializeHeap(*heap_factory_);
  return ProcessTracefile();
}

absl::Status TracefileExecutor::ProcessTracefile() {
  for (TraceLine line : reader_) {
    switch (line.op) {
      case TraceLine::Op::kMalloc: {
        DEFINE_OR_RETURN(void*, ptr, Malloc(line.input_size));

        if (line.input_size != 0) {
          auto [it, inserted] = id_map_.insert({ line.result, ptr });
          if (!inserted) {
            return absl::InternalError(
                "Tracefile allocated duplicate pointer.");
          }
        }
        break;
      }
      case TraceLine::Op::kCalloc: {
        DEFINE_OR_RETURN(void*, ptr, Calloc(line.nmemb, line.input_size));

        if (line.nmemb != 0 && line.input_size != 0) {
          auto [it, inserted] = id_map_.insert({ line.result, ptr });
          if (!inserted) {
            return absl::InternalError(
                "Tracefile allocated duplicate pointer.");
          }
        }
        break;
      }
      case TraceLine::Op::kRealloc: {
        void* ptr;
        if (line.input_ptr != nullptr) {
          auto it = id_map_.find(line.input_ptr);
          if (it == id_map_.end()) {
            return absl::InternalError(
                "Tracefile realloced non-existent pointer.");
          }

          ASSIGN_OR_RETURN(ptr, Realloc(it->second, line.input_size));

          id_map_.erase(it);
        } else {
          ASSIGN_OR_RETURN(ptr, Realloc(nullptr, line.input_size));
        }

        auto [it, inserted] = id_map_.insert({ line.result, ptr });
        if (!inserted) {
          return absl::InternalError("Tracefile allocated duplicate pointer.");
        }
        break;
      }
      case TraceLine::Op::kFree: {
        if (line.input_ptr == nullptr) {
          RETURN_IF_ERROR(Free(nullptr));
          break;
        }

        auto it = id_map_.find(line.input_ptr);
        if (it == id_map_.end()) {
          return absl::InternalError(absl::StrFormat(
              "Tracefile freed non-existent pointer %p.", line.input_ptr));
        }

        RETURN_IF_ERROR(Free(it->second));

        id_map_.erase(it);
        break;
      }
    }
  }

  return absl::OkStatus();
}

}  // namespace bench

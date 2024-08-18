#include "src/tracefile_executor.h"

#include <optional>

#include "absl/status/status.h"
#include "util/absl_util.h"

#include "src/tracefile_reader.h"

namespace bench {

TracefileExecutor::TracefileExecutor(TracefileReader&& reader)
    : reader_(std::move(reader)) {}

absl::Status TracefileExecutor::Run() {
  InitializeHeap();
  return ProcessTracefile();
}

absl::Status TracefileExecutor::ProcessTracefile() {
  while (true) {
    DEFINE_OR_RETURN(std::optional<TraceLine>, line, reader_.NextLine());
    if (!line.has_value()) {
      break;
    }

    switch (line->op) {
      case TraceLine::Op::kMalloc: {
        DEFINE_OR_RETURN(void*, ptr, Malloc(line->input_size));

        if (line->input_size != 0) {
          auto [it, inserted] = id_map_.insert({ line->result, ptr });
          if (!inserted) {
            return absl::InternalError(
                "Tracefile allocated duplicate pointer.");
          }
        }
        break;
      }
      case TraceLine::Op::kCalloc: {
        DEFINE_OR_RETURN(void*, ptr, Calloc(line->nmemb, line->input_size));

        if (line->nmemb != 0 && line->input_size != 0) {
          auto [it, inserted] = id_map_.insert({ line->result, ptr });
          if (!inserted) {
            return absl::InternalError(
                "Tracefile allocated duplicate pointer.");
          }
        }
        break;
      }
      case TraceLine::Op::kRealloc: {
        void* ptr;
        if (line->input_ptr != nullptr) {
          auto it = id_map_.find(line->input_ptr);
          if (it == id_map_.end()) {
            return absl::InternalError(
                "Tracefile realloced non-existent pointer.");
          }

          ASSIGN_OR_RETURN(ptr, Realloc(it->second, line->input_size));

          id_map_.erase(it);
        } else {
          ptr = nullptr;
        }

        auto [it, inserted] = id_map_.insert({ line->result, ptr });
        if (!inserted) {
          return absl::InternalError("Tracefile allocated duplicate pointer.");
        }
        break;
      }
      case TraceLine::Op::kFree: {
        if (line->input_ptr == nullptr) {
          RETURN_IF_ERROR(Free(nullptr));
          break;
        }

        auto it = id_map_.find(line->input_ptr);
        if (it == id_map_.end()) {
          return absl::InternalError("Tracefile freed non-existent pointer.");
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

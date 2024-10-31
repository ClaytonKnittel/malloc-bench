#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <ostream>
#include <unistd.h>

#include "absl/container/btree_set.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/text_format.h"
#include "re2/re2.h"
#include "util/absl_util.h"

#include "proto/tracefile.pb.h"

ABSL_FLAG(std::string, trace, "", "File path of the trace to clean.");

ABSL_FLAG(bool, binary, false, "Output binary proto");

ABSL_FLAG(uint64_t, max_ops, std::numeric_limits<uint64_t>::max(),
          "Limits the total number of ops in a trace. A tracefile will stop "
          "being parsed after enough ops have been parsed, accounting for "
          "needing to free all allocated memory.");

namespace bench {

using proto::Tracefile;
using proto::TraceLine;

namespace {

using re2::LazyRE2;
using re2::RE2;

/**
 * Matches a single line of valigrind --trace-malloc=yes output.
 *
 * Lines have one of the following formats:
 *
 * --{pid}-- free({ptr})
 * --{pid}-- malloc({size}) = {ptr}
 * --{pid}-- calloc({size},{nmemb}) = {ptr}
 * --{pid}-- realloc({ptr},{size}) = {ptr}
 * --{pid}-- realloc(0x0,{size})malloc({size}) = {ptr}
 *
 * Where pid, size, and nmemb are decimal numbers, and ptr is a hex value.
 *
 * "free" has aliases "_ZdlPv", "_ZdaPv", "_ZdlPvm", "_ZdaPvm", and
 * "malloc" has aliases "_Znwm", "_Znam".
 */
constexpr LazyRE2 kFreeRegex = {
  R"(--(\d+)-- (?:free|_ZdlPv|_ZdaPv|_ZdlPvm|_ZdaPvm|_ZdlPvSt11align_val_t)\(0x([0-9A-Fa-f]+)\))"
};
constexpr LazyRE2 kMallocRegex = {
  R"(--(\d+)-- (?:malloc|_Znwm|_Znam|_ZnwmRKSt9nothrow_t)\((\d+)\) = 0x([0-9A-Fa-f]+))"
};
constexpr LazyRE2 kMemalignAllocRegex = {
  R"(--(\d+)-- (?:memalign)\(al (\d+), size (\d+)\) = 0x([0-9A-Fa-f]+))"
};
constexpr LazyRE2 kAlignedAllocRegex = {
  R"(--(\d+)-- (?:_ZnwmSt11align_val_t)\(size (\d+), al (\d+)\) = 0x([0-9A-Fa-f]+))"
};
constexpr LazyRE2 kCallocRegex = {
  R"(--(\d+)-- calloc\((\d+),(\d+)\) = 0x([0-9A-Fa-f]+))"
};
constexpr LazyRE2 kReallocRegex = {
  R"(--(\d+)-- realloc\(0x([0-9A-Fa-f]+),(\d+)\)(?:malloc\(\d+\))? = 0x([0-9A-Fa-f]+))"
};

}  // namespace

class DirtyTracefileReader {
 public:
  using const_iterator =
      google::protobuf::internal::RepeatedPtrIterator<const TraceLine>;

  static absl::StatusOr<DirtyTracefileReader> Open(
      const std::string& filename) {
    std::ifstream file(filename);

    if (!file.is_open()) {
      return absl::InternalError(
          absl::StrCat("Failed to open file ", filename));
    }

    class Tracefile tracefile;
    std::optional<int32_t> required_pid;
    absl::flat_hash_map<void*, uint64_t> id_map;
    absl::btree_set<uint64_t> available_ids;
    uint64_t next_id = 0;
    uint64_t max_simultaneous_allocs = 0;

    const auto get_next_id = [&available_ids, &next_id,
                              &id_map](void* ptr) -> absl::StatusOr<uint64_t> {
      uint64_t id;
      if (available_ids.empty()) {
        id = next_id++;
      } else {
        auto it = available_ids.begin();
        id = *it;
        available_ids.erase(it);
      }

      auto [_it, inserted] = id_map.emplace(ptr, id);
      if (!inserted) {
        return absl::FailedPreconditionError(
            absl::StrFormat("Allocated duplicate pointer %p", ptr));
      }
      return id;
    };
    const auto find_and_erase_id =
        [&available_ids, &id_map](void* ptr) -> absl::StatusOr<uint64_t> {
      auto it = id_map.find(ptr);
      if (it == id_map.end()) {
        return absl::FailedPreconditionError(
            absl::StrFormat("Tracefile frees unallocated ptr %p", ptr));
      }
      uint64_t id = it->second;
      id_map.erase(it);

      auto [_it, inserted] = available_ids.insert(id);
      assert(inserted);
      return id;
    };

    for (uint64_t iter = 0; iter + id_map.size() < absl::GetFlag(FLAGS_max_ops);
         iter++) {
      std::string line_str;
      if (std::getline(file, line_str).eof()) {
        break;
      }

      TraceLine line;
      int32_t pid;
      absl::string_view matches[4];
      if (RE2::PartialMatch(line_str, *kFreeRegex, &matches[0], &matches[1])) {
        ASSIGN_OR_RETURN(pid, ParsePid(matches[0]));

        void* input_ptr;
        ASSIGN_OR_RETURN(input_ptr, ParsePtr(matches[1]));

        proto::TraceLine_Free* free = line.mutable_free();
        if (input_ptr != nullptr) {
          DEFINE_OR_RETURN(uint64_t, id, find_and_erase_id(input_ptr));
          free->set_input_id(id);
        }
      } else if (RE2::PartialMatch(line_str, *kMallocRegex, &matches[0],
                                   &matches[1], &matches[2])) {
        ASSIGN_OR_RETURN(pid, ParsePid(matches[0]));

        size_t input_size;
        ASSIGN_OR_RETURN(input_size, ParseSize(matches[1]));
        void* result_ptr;
        ASSIGN_OR_RETURN(result_ptr, ParsePtr(matches[2]));

        DEFINE_OR_RETURN(uint64_t, id, get_next_id(result_ptr));
        proto::TraceLine_Malloc* malloc = line.mutable_malloc();
        malloc->set_input_size(input_size);
        malloc->set_result_id(id);
      } else if (RE2::PartialMatch(line_str, *kMemalignAllocRegex, &matches[0],
                                   &matches[1], &matches[2], &matches[3])) {
        ASSIGN_OR_RETURN(pid, ParsePid(matches[0]));

        size_t input_alignment;
        ASSIGN_OR_RETURN(input_alignment, ParseSize(matches[1]));
        size_t input_size;
        ASSIGN_OR_RETURN(input_size, ParseSize(matches[2]));
        void* result_ptr;
        ASSIGN_OR_RETURN(result_ptr, ParsePtr(matches[3]));

        DEFINE_OR_RETURN(uint64_t, id, get_next_id(result_ptr));
        proto::TraceLine_Malloc* malloc = line.mutable_malloc();
        malloc->set_input_size(input_size);
        malloc->set_input_alignment(input_alignment);
        malloc->set_result_id(id);
      } else if (RE2::PartialMatch(line_str, *kAlignedAllocRegex, &matches[0],
                                   &matches[1], &matches[2], &matches[3])) {
        ASSIGN_OR_RETURN(pid, ParsePid(matches[0]));

        size_t input_size;
        ASSIGN_OR_RETURN(input_size, ParseSize(matches[1]));
        size_t input_alignment;
        ASSIGN_OR_RETURN(input_alignment, ParseSize(matches[2]));
        void* result_ptr;
        ASSIGN_OR_RETURN(result_ptr, ParsePtr(matches[3]));

        DEFINE_OR_RETURN(uint64_t, id, get_next_id(result_ptr));
        proto::TraceLine_Malloc* malloc = line.mutable_malloc();
        malloc->set_input_size(input_size);
        malloc->set_input_alignment(input_alignment);
        malloc->set_result_id(id);
      } else if (RE2::PartialMatch(line_str, *kCallocRegex, &matches[0],
                                   &matches[1], &matches[2], &matches[3])) {
        ASSIGN_OR_RETURN(pid, ParsePid(matches[0]));

        size_t nmemb;
        ASSIGN_OR_RETURN(nmemb, ParseSize(matches[1]));
        size_t input_size;
        ASSIGN_OR_RETURN(input_size, ParseSize(matches[2]));
        void* result_ptr;
        ASSIGN_OR_RETURN(result_ptr, ParsePtr(matches[3]));

        DEFINE_OR_RETURN(uint64_t, id, get_next_id(result_ptr));
        proto::TraceLine_Calloc* calloc = line.mutable_calloc();
        calloc->set_input_nmemb(nmemb);
        calloc->set_input_size(input_size);
        calloc->set_result_id(id);
      } else if (RE2::PartialMatch(line_str, *kReallocRegex, &matches[0],
                                   &matches[1], &matches[2], &matches[3])) {
        ASSIGN_OR_RETURN(pid, ParsePid(matches[0]));

        void* input_ptr;
        ASSIGN_OR_RETURN(input_ptr, ParsePtr(matches[1]));
        size_t input_size;
        ASSIGN_OR_RETURN(input_size, ParseSize(matches[2]));
        void* result_ptr;
        ASSIGN_OR_RETURN(result_ptr, ParsePtr(matches[3]));

        proto::TraceLine_Realloc* realloc = line.mutable_realloc();
        if (input_ptr != nullptr) {
          DEFINE_OR_RETURN(uint64_t, id, find_and_erase_id(input_ptr));
          realloc->set_input_id(id);
        }
        DEFINE_OR_RETURN(uint64_t, id, get_next_id(result_ptr));
        realloc->set_input_size(input_size);
        realloc->set_result_id(id);
      } else {
        std::cerr << "Skipping line " << line_str << std::endl;
        continue;
      }

      if (!required_pid.has_value()) {
        // Assume the main process makes the first allocation.
        required_pid = pid;
      } else if (required_pid.value() != pid) {
        continue;
      }
      *tracefile.mutable_lines()->Add() = std::move(line);

      max_simultaneous_allocs =
          std::max<uint64_t>(max_simultaneous_allocs, id_map.size());
    }

    file.close();

    // Free all unfreed memory.
    for (auto [ptr, id] : id_map) {
      TraceLine* line = tracefile.mutable_lines()->Add();
      proto::TraceLine_Free* free = line->mutable_free();
      free->set_input_id(id);
    }

    tracefile.set_max_simultaneous_allocs(max_simultaneous_allocs);
    return DirtyTracefileReader(std::move(tracefile));
  }

  const Tracefile& Tracefile() const {
    return tracefile_;
  }

 private:
  explicit DirtyTracefileReader(class Tracefile&& tracefile)
      : tracefile_(std::move(tracefile)) {}

  static absl::StatusOr<int32_t> ParsePid(absl::string_view spid) {
    int32_t pid;
    if (!absl::SimpleAtoi(spid, &pid)) {
      return absl::InternalError(
          absl::StrCat("failed to parse ", spid, " as int32"));
    }
    return pid;
  }
  static absl::StatusOr<void*> ParsePtr(absl::string_view sptr) {
    uintptr_t ptr_val;
    if (!absl::SimpleHexAtoi(sptr, &ptr_val)) {
      return absl::InternalError(
          absl::StrCat("failed to parse 0x", sptr, " as void*"));
    }
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    return reinterpret_cast<void*>(ptr_val);
  }

  static absl::StatusOr<size_t> ParseSize(absl::string_view ssize) {
    size_t size;
    if (!absl::SimpleAtoi(ssize, &size)) {
      return absl::InternalError(
          absl::StrCat("failed to parse ", ssize, " as size_t"));
    }
    return size;
  }

  const class Tracefile tracefile_;
};

absl::Status CleanTracefile(absl::string_view input_path, std::ostream& out,
                            bool text_serialize = false) {
  DEFINE_OR_RETURN(DirtyTracefileReader, reader,
                   DirtyTracefileReader::Open(std::string(input_path)));
  if (text_serialize) {
    google::protobuf::io::OstreamOutputStream os(&out);
    google::protobuf::TextFormat::Print(reader.Tracefile(), &os);
  } else {
    reader.Tracefile().SerializeToOstream(&out);
  }

  return absl::OkStatus();
}

}  // namespace bench

int main(int argc, char* argv[]) {
  absl::ParseCommandLine(argc, argv);

  const std::string& input_tracefile_path = absl::GetFlag(FLAGS_trace);
  if (input_tracefile_path.empty()) {
    std::cerr << "Flag --input is required" << std::endl;
  }

  absl::Status s =
      bench::CleanTracefile(input_tracefile_path, std::cout,
                            /*text_serialize=*/!absl::GetFlag(FLAGS_binary));

  if (!s.ok()) {
    std::cerr << "Fatal error: " << s << std::endl;
  }

  return 0;
}

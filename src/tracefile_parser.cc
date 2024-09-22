#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <ostream>
#include <regex>
#include <sstream>
#include <unistd.h>

#include "absl/container/btree_set.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/text_format.h"
#include "util/absl_util.h"

#include "proto/tracefile.pb.h"

ABSL_FLAG(std::string, trace, "", "File path of the trace to clean.");

ABSL_FLAG(bool, binary, false, "Output binary proto");

namespace bench {

using proto::Tracefile;
using proto::TraceLine;

namespace {

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
const std::regex kFreeRegex(
    R"(--(\d+)-- (?:free|_ZdlPv|_ZdaPv|_ZdlPvm|_ZdaPvm)\(([0-9A-Fa-fx]+)\))");
const std::regex kMallocRegex(
    R"(--(\d+)-- (?:malloc|_Znwm|_Znam)\((\d+)\) = ([0-9A-Fa-fx]+))");
const std::regex kCallocRegex(
    R"(--(\d+)-- calloc\((\d+),(\d+)\) = ([0-9A-Fa-fx]+))");
const std::regex kReallocRegex(
    R"(--(\d+)-- realloc\(([0-9A-Fa-fx]+),(\d+)\)(?:malloc\(\d+\))? = ([0-9A-Fa-fx]+))");

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

    while (true) {
      std::string line_str;
      if (std::getline(file, line_str).eof()) {
        break;
      }

      TraceLine line;
      std::smatch match;
      int32_t pid;
      if (std::regex_match(line_str, match, kFreeRegex)) {
        ASSIGN_OR_RETURN(pid, ParsePid(match[1].str()));

        void* input_ptr;
        ASSIGN_OR_RETURN(input_ptr, ParsePtr(match[2].str()));

        proto::TraceLine_Free* free = line.mutable_free();
        if (input_ptr != nullptr) {
          DEFINE_OR_RETURN(uint64_t, id, find_and_erase_id(input_ptr));
          free->set_input_id(id);
        }
      } else if (std::regex_match(line_str, match, kMallocRegex)) {
        ASSIGN_OR_RETURN(pid, ParsePid(match[1].str()));

        uint64_t input_size;
        ASSIGN_OR_RETURN(input_size, ParseSize(match[2].str()));
        void* result_ptr;
        ASSIGN_OR_RETURN(result_ptr, ParsePtr(match[3].str()));

        DEFINE_OR_RETURN(uint64_t, id, get_next_id(result_ptr));
        proto::TraceLine_Malloc* malloc = line.mutable_malloc();
        malloc->set_input_size(input_size);
        malloc->set_result_id(id);
      } else if (std::regex_match(line_str, match, kCallocRegex)) {
        ASSIGN_OR_RETURN(pid, ParsePid(match[1].str()));

        uint64_t nmemb;
        ASSIGN_OR_RETURN(nmemb, ParseSize(match[2].str()));
        uint64_t input_size;
        ASSIGN_OR_RETURN(input_size, ParseSize(match[3].str()));
        void* result_ptr;
        ASSIGN_OR_RETURN(result_ptr, ParsePtr(match[4].str()));

        DEFINE_OR_RETURN(uint64_t, id, get_next_id(result_ptr));
        proto::TraceLine_Calloc* calloc = line.mutable_calloc();
        calloc->set_input_nmemb(nmemb);
        calloc->set_input_size(input_size);
        calloc->set_result_id(id);
      } else if (std::regex_match(line_str, match, kReallocRegex)) {
        ASSIGN_OR_RETURN(pid, ParsePid(match[1].str()));

        void* input_ptr;
        ASSIGN_OR_RETURN(input_ptr, ParsePtr(match[2].str()));
        uint64_t input_size;
        ASSIGN_OR_RETURN(input_size, ParseSize(match[3].str()));
        void* result_ptr;
        ASSIGN_OR_RETURN(result_ptr, ParsePtr(match[4].str()));

        proto::TraceLine_Realloc* realloc = line.mutable_realloc();
        if (input_ptr != nullptr) {
          DEFINE_OR_RETURN(uint64_t, id, find_and_erase_id(input_ptr));
          realloc->set_input_id(id);
        }
        DEFINE_OR_RETURN(uint64_t, id, get_next_id(result_ptr));
        realloc->set_input_size(input_size);
        realloc->set_result_id(id);
      } else {
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

  template <typename T>
  static absl::StatusOr<T> ParsePrimitive(const std::string& str,
                                          const std::string& debug_name) {
    T val;
    if (!(std::istringstream(str) >> val).eof()) {
      return absl::InternalError(
          absl::StrCat("failed to parse ", str, " as ", debug_name));
    }
    return val;
  }

  static absl::StatusOr<int32_t> ParsePid(const std::string& spid) {
    return ParsePrimitive<int32_t>(spid, "pid");
  }
  static absl::StatusOr<void*> ParsePtr(const std::string& sptr) {
    return ParsePrimitive<void*>(sptr, "pointer");
  }

  static absl::StatusOr<size_t> ParseSize(const std::string& ssize) {
    return ParsePrimitive<size_t>(ssize, "size");
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

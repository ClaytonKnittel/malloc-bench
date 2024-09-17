#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <ios>
#include <limits>
#include <optional>
#include <string>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "util/absl_util.h"
#include "util/csi.h"
#include "util/print_colors.h"

#include "src/ckmalloc/ckmalloc.h"
#include "src/ckmalloc/common.h"
#include "src/ckmalloc/global_state.h"
#include "src/ckmalloc/heap_printer.h"
#include "src/ckmalloc/local_cache.h"
#include "src/heap_factory.h"
#include "src/mmap_heap_factory.h"
#include "src/tracefile_executor.h"
#include "src/tracefile_reader.h"

ABSL_FLAG(std::string, trace, "",
          "A path to the tracefile to run (must start with \"traces/\").");

ABSL_FLAG(bool, test_run, false,
          "If set, instead of printing to the screen, the program will "
          "silently render the heap after every allocation in the background. "
          "Used for debugging.");

ABSL_FLAG(bool, to_max, false,
          "If set, searches for the point in the tracefile with the maximum "
          "amount of allocated memory, and immediately jumps to that point.");

#define ALLOC_COLOR  P_GREEN
#define FREE_COLOR   P_RED
#define CACHED_COLOR P_YELLOW

namespace ckmalloc {

using bench::HeapFactory;
using bench::TracefileExecutor;
using bench::TracefileReader;
using Op = bench::TraceLine::Op;

class FindMaxAllocations : public TracefileExecutor {
 public:
  FindMaxAllocations(TracefileReader&& reader, HeapFactory& heap_factory)
      : TracefileExecutor(std::move(reader), heap_factory) {}

  absl::StatusOr<uint64_t> MaxAllocations() {
    if (!iters_to_max_.has_value()) {
      RETURN_IF_ERROR(TracefileExecutor::Run());
      iters_to_max_ = max_iter_;
    }

    return iters_to_max_.value();
  }

  void InitializeHeap(HeapFactory& heap_factory) override {
    CkMalloc::InitializeHeap(heap_factory);
  }

  absl::StatusOr<void*> Malloc(size_t size) override {
    iter_++;
    void* result = CkMalloc::Instance()->Malloc(size);
    if (result != nullptr) {
      alloc_sizes_[result] = size;
      total_allocations_ += size;
      if (total_allocations_ > max_allocations_) {
        max_allocations_ = total_allocations_;
        max_iter_ = iter_;
      }
    }
    return result;
  }

  absl::StatusOr<void*> Calloc(size_t nmemb, size_t size) override {
    iter_++;
    void* result = CkMalloc::Instance()->Calloc(nmemb, size);
    if (result != nullptr) {
      alloc_sizes_[result] = size;
      total_allocations_ += size;
      if (total_allocations_ > max_allocations_) {
        max_allocations_ = total_allocations_;
        max_iter_ = iter_;
      }
    }
    return result;
  }

  absl::StatusOr<void*> Realloc(void* ptr, size_t size) override {
    iter_++;
    if (ptr != nullptr) {
      auto it = alloc_sizes_.find(ptr);
      total_allocations_ -= it->second;
      alloc_sizes_.erase(it);
    }

    void* result = CkMalloc::Instance()->Realloc(ptr, size);
    if (result != nullptr) {
      alloc_sizes_[result] = size;
      total_allocations_ += size;
      if (total_allocations_ > max_allocations_) {
        max_allocations_ = total_allocations_;
        max_iter_ = iter_;
      }
    }
    return result;
  }

  absl::Status Free(void* ptr) override {
    iter_++;
    if (ptr != nullptr) {
      auto it = alloc_sizes_.find(ptr);
      total_allocations_ -= it->second;
      alloc_sizes_.erase(it);
    }
    CkMalloc::Instance()->Free(ptr);
    return absl::OkStatus();
  }

 private:
  absl::flat_hash_map<void*, size_t> alloc_sizes_;
  uint64_t iter_ = 0;
  uint64_t total_allocations_ = 0;
  uint64_t max_iter_ = 0;
  uint64_t max_allocations_ = 0;
  std::optional<uint64_t> iters_to_max_;
};

struct TraceOp {
  Op op;
  // For free/realloc, the input pointer.
  void* input_ptr;
  // For calloc, the requested nmemb.
  size_t input_nmemb;
  // For malloc/calloc/realloc/free_hint, the requested size.
  size_t input_size;

  // For malloc/calloc/realloc, the result of this operation (only defined after
  // the operation has occurred).
  void* result;
};

class TraceReplayer : public TracefileExecutor {
 public:
  TraceReplayer(TracefileReader&& reader, HeapFactory& heap_factory)
      : TracefileExecutor(std::move(reader), heap_factory),
        heap_factory_(&heap_factory) {
    if (!absl::GetFlag(FLAGS_test_run)) {
      std::cout << CSI_ALTERNATE_DISPLAY << CSI_HIDE << CSI_CHP(1, 1);
      SetNonCanonicalMode(/*enable=*/true);
    }
  }

  ~TraceReplayer() {
    if (!absl::GetFlag(FLAGS_test_run)) {
      SetNonCanonicalMode(/*enable=*/false);
      std::cout << CSI_SHOW << CSI_MAIN_DISPLAY;
    }
  }

  void SetSkips(uint64_t skips) {
    skips_ = skips;
  }

  absl::Status SetDone() {
    if (!done_) {
      skips_ = 0;
      RETURN_IF_ERROR(RefreshPrintedHeap());
    }
    done_ = true;
    return absl::OkStatus();
  }

  void InitializeHeap(HeapFactory& heap_factory) override {
    CkMalloc::InitializeHeap(heap_factory);
  }

  absl::StatusOr<void*> Malloc(size_t size) override {
    prev_op_ = next_op_;
    next_op_ = {
      .op = Op::kMalloc,
      .input_size = size,
    };
    RETURN_IF_ERROR(RefreshPrintedHeap());

    RETURN_IF_ERROR(AwaitInput());

    DEFINE_OR_RETURN(void*, result, CkMalloc::Instance()->Malloc(size));
    next_op_.result = result;
    return result;
  }

  absl::StatusOr<void*> Calloc(size_t nmemb, size_t size) override {
    prev_op_ = next_op_;
    next_op_ = {
      .op = Op::kCalloc,
      .input_nmemb = nmemb,
      .input_size = size,
    };
    RETURN_IF_ERROR(RefreshPrintedHeap());

    RETURN_IF_ERROR(AwaitInput());

    DEFINE_OR_RETURN(void*, result, CkMalloc::Instance()->Calloc(nmemb, size));
    next_op_.result = result;
    return result;
  }

  absl::StatusOr<void*> Realloc(void* ptr, size_t size) override {
    prev_op_ = next_op_;
    next_op_ = {
      .op = Op::kRealloc,
      .input_ptr = ptr,
      .input_size = size,
    };
    RETURN_IF_ERROR(RefreshPrintedHeap());

    RETURN_IF_ERROR(AwaitInput());

    DEFINE_OR_RETURN(void*, result, CkMalloc::Instance()->Realloc(ptr, size));
    next_op_.result = result;
    return result;
  }

  absl::Status Free(void* ptr) override {
    prev_op_ = next_op_;
    next_op_ = {
      .op = Op::kFree,
      .input_ptr = ptr,
    };
    RETURN_IF_ERROR(RefreshPrintedHeap());

    RETURN_IF_ERROR(AwaitInput());

    CkMalloc::Instance()->Free(ptr);
    return absl::OkStatus();
  }

  absl::Status AwaitInput() {
    if (absl::GetFlag(FLAGS_test_run)) {
      return absl::OkStatus();
    }

    if (!done_) {
      iter_++;
      if (skips_ != 0) {
        skips_--;
        return absl::OkStatus();
      }
    }

    while (true) {
      RETURN_IF_ERROR(Display());
      DEFINE_OR_RETURN(uint16_t, term_height, TermHeight());

      char c = getchar();
      if (c == 'n') {
        break;
      }
      if (c == 'm') {
        skips_ = 49;
        break;
      }
      if (c == 'c') {
        skips_ = 1023;
        break;
      }
      if (c == 'r') {
        skips_ = 9999;
        break;
      }

      switch (c) {
        case 'q': {
          return absl::AbortedError("User pressed 'q'");
        }
        case 'j': {
          ScrollBy(1, term_height);
          break;
        }
        case 'd': {
          ScrollBy((term_height - kUiLines) / 2, term_height);
          break;
        }
        case 'k': {
          ScrollBy(-1, term_height);
          break;
        }
        case 'u': {
          ScrollBy(-(term_height - kUiLines) / 2, term_height);
          break;
        }
        case '0':
        case '1': {
          size_t idx = static_cast<size_t>(c) - static_cast<size_t>('0');
          if (heap_factory_->Instance(idx) != nullptr) {
            heap_idx_ = idx;
            RETURN_IF_ERROR(RefreshPrintedHeap());
          }
          break;
        }
        default: {
          break;
        }
      }
    }

    return absl::OkStatus();
  }

 private:
  static constexpr size_t kUiLines = 2;

  static void SetNonCanonicalMode(bool enable) {
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);

    if (enable) {
      t.c_lflag &= ~ICANON;  // Disable canonical mode
      t.c_lflag &= ~ECHO;    // Disable echo
    } else {
      t.c_lflag |= ICANON;  // Enable canonical mode
      t.c_lflag |= ECHO;    // Enable echo
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &t);
  }

  absl::Status Display() {
    DEFINE_OR_RETURN(uint16_t, term_height, TermHeight());
    if (absl::GetFlag(FLAGS_test_run)) {
      term_height = std::numeric_limits<uint16_t>::max();
    }

    if (!absl::GetFlag(FLAGS_test_run)) {
      std::cout << CSI_CHP(1, 1) << CSI_ED(CSI_CURSOR_ALL);
    }

    std::cout
        << "Next: [n/m(50)/c(1024)/r(10000)], scroll down: [j/d], scroll up: "
           "[k/u], heap index: [0/1/...], quit: [q]"
        << std::endl;

    std::cout << "Next op: " << std::left << std::setw(28);
    switch (next_op_.op) {
      case Op::kMalloc: {
        std::cout << absl::StrFormat("malloc(%zu)", next_op_.input_size);
        break;
      }
      case Op::kCalloc: {
        std::cout << absl::StrFormat("calloc(%zu, %zu)", next_op_.input_nmemb,
                                     next_op_.input_size);
        break;
      }
      case Op::kRealloc: {
        std::cout << absl::StrFormat("realloc(%p, %zu)", next_op_.input_ptr,
                                     next_op_.input_size);
        break;
      }
      case Op::kFree: {
        std::cout << absl::StrFormat("free(%p)", next_op_.input_ptr);
        break;
      }
    }
    std::cout << " (" << iter_ << ")" << std::endl;

    uint32_t end = std::min<uint32_t>(printed_heap_.size(),
                                      scroll_ + term_height - kUiLines);
    for (uint32_t i = scroll_; i < end; i++) {
      std::cout << printed_heap_[i];
      if (i != end - 1) {
        std::cout << std::endl;
      }
    }

    return absl::OkStatus();
  }

  static absl::StatusOr<uint16_t> TermHeight() {
    struct winsize w;

    // Retrieve the terminal window size
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
      return absl::InternalError("Unable to get terminal size");
    }

    return w.ws_row;
  }

  uint32_t MaxScroll(uint16_t term_height) const {
    return std::max<uint32_t>(term_height - kUiLines, printed_heap_.size()) -
           (term_height - kUiLines);
  }

  void ScrollBy(int32_t diff, uint16_t term_height) {
    scroll_ = std::clamp<int32_t>(scroll_ + diff, 0, MaxScroll(term_height));
  }

  absl::Status RefreshPrintedHeap() {
    if (skips_ != 0) {
      return absl::OkStatus();
    }

    DEFINE_OR_RETURN(uint16_t, term_height, TermHeight());

    HeapPrinter p(heap_factory_->Instance(heap_idx_),
                  CkMalloc::Instance()->GlobalState()->SlabMap(),
                  CkMalloc::Instance()->GlobalState()->SlabManager(),
                  CkMalloc::Instance()->GlobalState()->MetadataManager());

    if (prev_op_.has_value() && prev_op_->op != Op::kFree) {
      p.WithHighlightAddr(prev_op_->result, ALLOC_COLOR);
    }
    if (next_op_.op == Op::kFree || next_op_.op == Op::kRealloc) {
      p.WithHighlightAddr(next_op_.input_ptr, FREE_COLOR);
    }

    // Iterate over all cached guys.
    for (auto* bin : LocalCache::Instance<GlobalMetadataAlloc>()->bins_) {
      for (auto* alloc = bin; alloc != nullptr; alloc = alloc->next) {
        p.WithHighlightAddr(alloc, CACHED_COLOR);
      }
    }

    std::string print = p.Print();

    printed_heap_ = absl::StrSplit(print, '\n');
    scroll_ = std::min(scroll_, MaxScroll(term_height));

    return absl::OkStatus();
  }

  HeapFactory* const heap_factory_;

  // The op about to be executed.
  TraceOp next_op_;
  // The op that was just executed. If nullopt, then there was no previous op
  // (i.e. `next_op_` is the first op in the trace).
  std::optional<TraceOp> prev_op_;

  // Which heap we are currently looking at.
  size_t heap_idx_ = 1;

  uint64_t iter_ = 0;
  uint64_t skips_ = 0;

  bool done_ = false;

  std::vector<std::string> printed_heap_;
  uint32_t scroll_ = 0;
};  // namespace ckmalloc

absl::Status Run(const std::string& tracefile) {
  uint64_t skips = 0;
  if (absl::GetFlag(FLAGS_to_max)) {
    DEFINE_OR_RETURN(TracefileReader, reader, TracefileReader::Open(tracefile));
    bench::MMapHeapFactory heap_factory;
    std::cout << "starting" << std::endl;
    ASSIGN_OR_RETURN(
        skips,
        FindMaxAllocations(std::move(reader), heap_factory).MaxAllocations());
    std::cout << "finished " << skips << std::endl;
  }

  DEFINE_OR_RETURN(TracefileReader, reader, TracefileReader::Open(tracefile));
  bench::MMapHeapFactory heap_factory;
  TraceReplayer replayer(std::move(reader), heap_factory);
  replayer.SetSkips(skips);
  RETURN_IF_ERROR(replayer.Run());
  RETURN_IF_ERROR(replayer.SetDone());
  while (true) {
    RETURN_IF_ERROR(replayer.AwaitInput());
  }
}

}  // namespace ckmalloc

int main(int argc, char* argv[]) {
  absl::ParseCommandLine(argc, argv);

  const std::string& tracefile = absl::GetFlag(FLAGS_trace);
  if (tracefile.empty()) {
    std::cerr << "Must specify --trace" << std::endl;
    return -1;
  }

  absl::Status result = ckmalloc::Run(tracefile);

  if (!result.ok()) {
    if (result.code() == absl::StatusCode::kAborted) {
      return 0;
    }

    std::cerr << result << std::endl;
  }

  return result.raw_code();
}

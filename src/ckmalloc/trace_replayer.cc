#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <ios>
#include <string>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "util/absl_util.h"
#include "util/csi.h"

#include "src/ckmalloc/ckmalloc.h"
#include "src/ckmalloc/heap_printer.h"
#include "src/ckmalloc/state.h"
#include "src/heap_factory.h"
#include "src/mmap_heap_factory.h"
#include "src/tracefile_executor.h"
#include "src/tracefile_reader.h"

ABSL_FLAG(std::string, trace, "",
          "A path to the tracefile to run (must start with \"traces/\").");

namespace ckmalloc {

using bench::HeapFactory;
using bench::TracefileExecutor;
using bench::TracefileReader;

class TraceReplayer : public TracefileExecutor {
 public:
  TraceReplayer(TracefileReader&& reader, HeapFactory& heap_factory)
      : TracefileExecutor(std::move(reader), heap_factory),
        heap_factory_(&heap_factory) {
    std::cout << CSI_ALTERNATE_DISPLAY << CSI_HIDE << CSI_CHP(1, 1);
    SetNonCanonicalMode(/*enable=*/true);
  }

  ~TraceReplayer() {
    SetNonCanonicalMode(/*enable=*/false);
    std::cout << CSI_SHOW << CSI_MAIN_DISPLAY;
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
    State::InitializeWithEmptyHeap(&heap_factory);
  }

  absl::StatusOr<void*> Malloc(size_t size) override {
    RETURN_IF_ERROR(RefreshPrintedHeap());
    op_ = Op::kMalloc;
    input_size_ = size;

    RETURN_IF_ERROR(AwaitInput());

    DEFINE_OR_RETURN(void*, result, ckmalloc::malloc(size));
    return result;
  }

  absl::StatusOr<void*> Calloc(size_t nmemb, size_t size) override {
    RETURN_IF_ERROR(RefreshPrintedHeap());
    op_ = Op::kCalloc;
    input_nmemb_ = nmemb;
    input_size_ = size;

    RETURN_IF_ERROR(AwaitInput());

    DEFINE_OR_RETURN(void*, result, ckmalloc::calloc(nmemb, size));
    return result;
  }

  absl::StatusOr<void*> Realloc(void* ptr, size_t size) override {
    RETURN_IF_ERROR(RefreshPrintedHeap());
    op_ = Op::kRealloc;
    input_ptr_ = ptr;
    input_size_ = size;

    RETURN_IF_ERROR(AwaitInput());

    DEFINE_OR_RETURN(void*, result, ckmalloc::realloc(ptr, size));
    return result;
  }

  absl::Status Free(void* ptr) override {
    RETURN_IF_ERROR(RefreshPrintedHeap());
    op_ = Op::kFree;
    input_ptr_ = ptr;

    RETURN_IF_ERROR(AwaitInput());

    ckmalloc::free(ptr);
    return absl::OkStatus();
  }

  absl::Status AwaitInput() {
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
        default: {
          break;
        }
      }
    }

    return absl::OkStatus();
  }

 private:
  static constexpr size_t kUiLines = 2;

  enum class Op {
    kMalloc,
    kCalloc,
    kRealloc,
    kFree,
  };

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

    std::cout << CSI_CHP(1, 1) << CSI_ED(CSI_CURSOR_ALL);

    std::cout << "Next: [n], scroll down: [j], scroll up: [k], quit: [q]"
              << std::endl;

    std::cout << "Next op: " << std::left << std::setw(28);
    switch (op_) {
      case Op::kMalloc: {
        std::cout << absl::StrFormat("malloc(%zu)", input_size_);
        break;
      }
      case Op::kCalloc: {
        std::cout << absl::StrFormat("calloc(%zu, %zu)", input_nmemb_,
                                     input_size_);
        break;
      }
      case Op::kRealloc: {
        std::cout << absl::StrFormat("realloc(%p, %zu)", input_ptr_,
                                     input_size_);
        break;
      }
      case Op::kFree: {
        std::cout << absl::StrFormat("free(%p)", input_ptr_);
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

    HeapPrinter p(heap_factory_->Instance(0), State::Instance()->SlabMap(),
                  State::Instance()->SlabManager());
    std::string print = p.Print();

    printed_heap_ = absl::StrSplit(print, '\n');
    scroll_ = std::min(scroll_, MaxScroll(term_height));

    return absl::OkStatus();
  }

  HeapFactory* const heap_factory_;

  // The op about to be executed.
  Op op_;
  // For free/realloc, the input pointer.
  void* input_ptr_;
  // For calloc, the requested nmemb.
  size_t input_nmemb_;
  // For malloc/calloc/realloc/free_hint, the requested size.
  size_t input_size_;

  uint64_t iter_ = 0;
  uint64_t skips_ = 0;

  bool done_ = false;

  std::vector<std::string> printed_heap_;
  uint32_t scroll_ = 0;
};

absl::Status Run(const std::string& tracefile) {
  DEFINE_OR_RETURN(TracefileReader, reader, TracefileReader::Open(tracefile));
  bench::MMapHeapFactory heap_factory;
  TraceReplayer replayer(std::move(reader), heap_factory);
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

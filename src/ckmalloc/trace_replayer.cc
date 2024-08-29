#include <cstdio>
#include <string>
#include <termios.h>
#include <unistd.h>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "util/absl_util.h"
#include "util/csi.h"

#include "src/ckmalloc/ckmalloc.h"
#include "src/ckmalloc/state.h"
#include "src/singleton_heap.h"
#include "src/tracefile_executor.h"
#include "src/tracefile_reader.h"

ABSL_FLAG(std::string, trace, "",
          "A path to the tracefile to run (must start with \"traces/\").");

namespace ckmalloc {

using bench::SingletonHeap;
using bench::TracefileExecutor;
using bench::TracefileReader;

class TraceReplayer : public TracefileExecutor {
 public:
  explicit TraceReplayer(TracefileReader&& reader)
      : TracefileExecutor(std::move(reader)) {
    std::cout << CSI_ALTERNATE_DISPLAY << CSI_HIDE << CSI_CHP(1, 1);
    SetNonCanonicalMode(/*enable=*/true);
  }

  ~TraceReplayer() {
    SetNonCanonicalMode(/*enable=*/false);
    std::cout << CSI_SHOW << CSI_MAIN_DISPLAY;
  }

  void InitializeHeap() override {
    SingletonHeap::GlobalInstance()->Reset();
    State::InitializeWithEmptyHeap(SingletonHeap::GlobalInstance());
  }

  absl::StatusOr<void*> Malloc(size_t size) override {
    op_ = Op::kMalloc;
    input_size_ = size;

    RETURN_IF_ERROR(AwaitInput());

    DEFINE_OR_RETURN(void*, result, ckmalloc::malloc(size));
    return result;
  }

  absl::StatusOr<void*> Calloc(size_t nmemb, size_t size) override {
    op_ = Op::kCalloc;
    input_nmemb_ = nmemb;
    input_size_ = size;

    RETURN_IF_ERROR(AwaitInput());

    DEFINE_OR_RETURN(void*, result, ckmalloc::calloc(nmemb, size));
    return result;
  }

  absl::StatusOr<void*> Realloc(void* ptr, size_t size) override {
    op_ = Op::kRealloc;
    input_ptr_ = ptr;
    input_size_ = size;

    RETURN_IF_ERROR(AwaitInput());

    DEFINE_OR_RETURN(void*, result, ckmalloc::realloc(ptr, size));
    return result;
  }

  absl::Status Free(void* ptr) override {
    op_ = Op::kFree;
    input_ptr_ = ptr;

    RETURN_IF_ERROR(AwaitInput());

    ckmalloc::free(ptr);
    return absl::OkStatus();
  }

 private:
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

  absl::Status AwaitInput() {
    while (true) {
      Display();
      char c = getchar();

      if (c == 'q') {
        return absl::AbortedError("User pressed 'q'");
      }
      if (c == 'n') {
        break;
      }
      if (c == 'j') {
      }
      if (c == 'k') {
      }
    }

    return absl::OkStatus();
  }

  void Display() {
    std::cout << CSI_CHP(1, 1);

    std::cout << "Next: [n], scroll down: [j], scroll up: [k], quit: [q]"
              << std::endl;

    std::cout << "Next op: ";
    switch (op_) {
      case Op::kMalloc: {
        std::cout << "malloc(" << input_size_ << ")" << std::endl;
        break;
      }
      case Op::kCalloc: {
        std::cout << "calloc(" << input_nmemb_ << ", " << input_size_ << ")"
                  << std::endl;
        break;
      }
      case Op::kRealloc: {
        std::cout << "realloc(" << input_ptr_ << ", " << input_size_ << ")"
                  << std::endl;
        break;
      }
      case Op::kFree: {
        std::cout << "free(" << input_ptr_ << ")" << std::endl;
        break;
      }
    }
  }

  // The op about to be executed.
  Op op_;
  // For free/realloc, the input pointer.
  void* input_ptr_;
  // For calloc, the requested nmemb.
  size_t input_nmemb_;
  // For malloc/calloc/realloc/free_hint, the requested size.
  size_t input_size_;

  std::vector<std::string> printed_heap_;
};

absl::Status Run(const std::string& tracefile) {
  DEFINE_OR_RETURN(TracefileReader, reader, TracefileReader::Open(tracefile));
  TraceReplayer replayer(std::move(reader));
  return replayer.Run();
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

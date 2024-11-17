#include "src/perfetto.h"

#ifdef PERFETTO_ENABLED

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <memory>

#include "absl/flags/flag.h"

#include "perfetto/tracing/core/trace_config.h"  // IWYU pragma: keep
#include "perfetto/tracing/tracing.h"

PERFETTO_TRACK_EVENT_STATIC_STORAGE();

ABSL_FLAG(std::string, perfetto_out, "./malloc-bench.perfetto-trace",
          "The file to write the output of the trace to, which can be read at "
          "https://ui.perfetto.dev.");

#endif /* PERFETTO_ENABLED */

namespace bench {

Perfetto::Perfetto() {
#ifdef PERFETTO_ENABLED
  perfetto::TracingInitArgs args;
  args.backends |= perfetto::kInProcessBackend;
  args.use_monotonic_clock = true;
  perfetto::Tracing::Initialize(args);
  perfetto::TrackEvent::Register();

  perfetto::protos::gen::TrackEventConfig track_event_cfg;
  track_event_cfg.add_disabled_categories("*");
  track_event_cfg.add_enabled_categories("test_infrastructure");

  perfetto::TraceConfig cfg;
  cfg.add_buffers()->set_size_kb(160 * 1024);  // Record up to 40 MiB.
  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("track_event");
  ds_cfg->set_track_event_config_raw(track_event_cfg.SerializeAsString());

  fd_ = open(absl::GetFlag(FLAGS_perfetto_out).c_str(),
             O_RDWR | O_CREAT | O_TRUNC, 0600);
  if (fd_ < 0) {
    std::cerr << "Failed to open " << absl::GetFlag(FLAGS_perfetto_out)
              << " for writing: " << strerror(errno) << std::endl;
    std::abort();
  }

  tracing_session_ = perfetto::Tracing::NewTrace();
  tracing_session_->Setup(cfg, fd_);
  tracing_session_->Start();
#endif /* PERFETTO_ENABLED */
}

Perfetto::~Perfetto() {
#ifdef PERFETTO_ENABLED
  tracing_session_->StopBlocking();
  close(fd_);
#endif /* PERFETTO_ENABLED */
}

}  // namespace bench

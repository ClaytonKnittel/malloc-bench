#include "src/perfetto.h"

#ifdef PERFETTO_ENABLED

#include <fcntl.h>
#include <memory>

#include "perfetto/tracing/core/trace_config.h"  // IWYU pragma: keep
#include "perfetto/tracing/tracing.h"

PERFETTO_TRACK_EVENT_STATIC_STORAGE();

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
  cfg.add_buffers()->set_size_kb(40 * 1024);  // Record up to 40 MiB.
  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("track_event");
  ds_cfg->set_track_event_config_raw(track_event_cfg.SerializeAsString());

  fd_ = open(
      "/home/cknittel/Documents/VSCode/malloc-bench/"
      "malloc-bench.perfetto-trace",
      O_RDWR | O_CREAT | O_TRUNC, 0600);

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

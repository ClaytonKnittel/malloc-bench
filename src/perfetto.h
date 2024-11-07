#pragma once

#include "perfetto/tracing/tracing.h"
#include "perfetto/tracing/track_event.h"

PERFETTO_DEFINE_CATEGORIES(
    perfetto::Category("test_infrastructure")
        .SetDescription("Events from testing infrastructure"));

namespace bench {

class Perfetto {
 public:
  Perfetto();
  ~Perfetto();

 private:
  std::unique_ptr<perfetto::TracingSession> tracing_session_;
  int fd_;
};

}  // namespace bench

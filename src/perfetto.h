#pragma once

#ifdef PERFETTO_ENABLED

#include "perfetto/tracing/tracing.h"
#include "perfetto/tracing/track_event.h"

PERFETTO_DEFINE_CATEGORIES(
    perfetto::Category("test_infrastructure")
        .SetDescription("Events from testing infrastructure"),
    perfetto::Category("ckmalloc").SetDescription("Events from ckmalloc"));

#else

#define TRACE_EVENT(category, name, ...)

#endif

namespace bench {

class Perfetto {
 public:
  Perfetto();
  ~Perfetto();

 private:
#ifdef PERFETTO_ENABLED
  std::unique_ptr<perfetto::TracingSession> tracing_session_;
  int fd_;
#endif
};

}  // namespace bench

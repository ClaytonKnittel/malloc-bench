#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace jsmalloc {

class FileLogger {
 public:
  enum class Level {
    kDebug = 0,
    kInfo = 1,
    kError = 2,
  };

  void Open(char const* file);

  void Log(Level level, const char* fmt, ...) const;

  void Flush() const;

 private:
  int fd_ = 0;
};

class GLogger {
 public:
  static FileLogger& Instance();

 private:
  static void Open();

  static bool opened_;
  static std::mutex mu_;
  static FileLogger logger_;
};

#define DLOG_INTERNAL(log_level, log_predicate, flush, ...)  \
  do {                                                       \
    auto DEBUG = ::jsmalloc::FileLogger::Level::kDebug;      \
    (void) DEBUG;                                            \
    auto INFO = ::jsmalloc::FileLogger::Level::kInfo;        \
    (void) INFO;                                             \
    auto ERROR = ::jsmalloc::FileLogger::Level::kError;      \
    (void) ERROR;                                            \
                                                             \
    ::jsmalloc::FileLogger::Level lvl = log_level;           \
    if (log_predicate) {                                     \
      ::jsmalloc::GLogger::Instance().Log(lvl, __VA_ARGS__); \
    }                                                        \
    if (flush) {                                             \
      ::jsmalloc::GLogger::Instance().Flush();               \
    }                                                        \
  } while (false);

#define DLOG_IF(level, cond, ...) DLOG_INTERNAL(level, cond, false, __VA_ARGS__)
#define DLOG(level, ...) DLOG_IF(level, true, __VA_ARGS__)

}  // namespace jsmalloc

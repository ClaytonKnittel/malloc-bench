#include "src/jsmalloc/util/file_logger.h"

#include <cstdarg>
#include <cstdio>
#include <thread>

namespace jsmalloc {

bool GLogger::opened_ = false;
std::mutex GLogger::mu_;
FileLogger GLogger::logger_;

void FileLogger::Open(char const* file) {
  fd_ = open(file, O_CREAT | O_WRONLY | O_TRUNC,
             S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
}

char const* LevelString(FileLogger::Level l) {
  switch (l) {
    case FileLogger::Level::kDebug:
      return "DEBUG";
    case FileLogger::Level::kInfo:
      return "INFO";
    case FileLogger::Level::kError:
      return "ERROR";
  }
}

void FileLogger::Log(Level level, const char* fmt, ...) const {
  va_list args;
  char buf[512];

#ifdef NDEBUG
  if (level == Level::kDebug) {
    return;
  }
#endif

  std::thread::id tid = std::this_thread::get_id();

  // It's kind of annoying to get the actual thread ID or stack start,
  // so we just pick a consistent, probably unique value.
  size_t tid_value = std::hash<std::thread::id>{}(tid) & ((1 << 30) - 1);
  int pos = std::sprintf(buf, "%s - tid:%zX - ", LevelString(level), tid_value);

  va_start(args, fmt);
  pos += std::vsprintf(&buf[pos], fmt, args);
  va_end(args);

  write(fd_, buf, strlen(buf));

  if (level == Level::kError) {
    fsync(fd_);
  }
}

void GLogger::Open() {
  std::lock_guard l(mu_);
  if (opened_) {
    return;
  }
  opened_ = true;

  char file[256];
  std::sprintf(file, "/tmp/glogger-%d.txt", ::getpid());
  logger_.Open(file);
}

FileLogger& GLogger::Instance() {
  Open();
  return logger_;
}

}  // namespace jsmalloc

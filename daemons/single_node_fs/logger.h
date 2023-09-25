#pragma once

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace chfs {

/**
 * Provide a logger abstraction
 * Such that the logs from the FUSE implementation can
 */
class Logger {
  FILE *logfile;

public:
  Logger(const std::string &filename) {
    this->logfile = fopen(filename.c_str(), "w");
    if (this->logfile == nullptr) {
      std::cerr << "Fatal error: failed to open the log file" << std::endl;
      std::abort();
    }
    // set logfile to line buffering
    setvbuf(logfile, NULL, _IOLBF, 0);
  }

  Logger &operator<<(const char *msg) {
    if (this->logfile != nullptr) {
      std::fprintf(this->logfile, "%s\n", msg);
      std::fflush(this->logfile); // Ensure the log is written immediately
    }
    return *this;
  }

  Logger &operator<<(const std::string &msg) {
    if (this->logfile != nullptr) {
      std::fprintf(this->logfile, "%s\n", msg.c_str());
      std::fflush(this->logfile); // Ensure the log is written immediately
    }
    return *this;
  }

  // Overload the << operator for log messages
  template <typename T> Logger &operator<<(const T &msg) {
    if (this->logfile != nullptr) {
      std::fprintf(this->logfile, "%s\n", std::to_string(msg).c_str());
      std::fflush(this->logfile); // Ensure the log is written immediately
    }
    return *this;
  }

  void log(const std::string &message) { (*this) << message << "\n"; }

  void close() {}
};

extern Logger logger; // Declaration of the global logger instance

#ifdef UNIMPLEMENTED
#undef UNIMPLEMENTED
#endif

#define UNIMPLEMENTED()                                                        \
  do {                                                                         \
    ::chfs::logger << "Unimplemented function of chfs: "                       \
                   << __PRETTY_FUNCTION__ << " at " << __FILE__ << ":"         \
                   << __LINE__ << "\n";                                        \
    std::abort();                                                              \
  } while (0)

} // namespace chfs
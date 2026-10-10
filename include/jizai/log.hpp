#pragma once

#include <iostream>
#include <ostream>

namespace jizai {

// Where the fitters, the solver and the preconditioner print their progress. Thread-local, so
// a program fitting on several threads at once can route each fit's output to its own sink;
// std::cout when nothing is set.
inline thread_local std::ostream* g_log_stream = nullptr;

inline std::ostream& log_stream() { return g_log_stream != nullptr ? *g_log_stream : std::cout; }

// Routes this thread's jizai output to a stream for the object's lifetime.
class ScopedLogStream {
 public:
  explicit ScopedLogStream(std::ostream& stream) : previous_(g_log_stream) {
    g_log_stream = &stream;
  }

  ~ScopedLogStream() { g_log_stream = previous_; }

  ScopedLogStream(const ScopedLogStream&) = delete;
  ScopedLogStream& operator=(const ScopedLogStream&) = delete;

 private:
  std::ostream* previous_;
};

}  // namespace jizai

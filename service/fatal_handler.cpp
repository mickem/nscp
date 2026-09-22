// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "fatal_handler.hpp"

#include <atomic>
#include <boost/thread.hpp>
#include <cstdlib>
#include <exception>
#include <nsclient/logger/logger_helper.hpp>
#include <sstream>
#include <string>
#include <typeinfo>

namespace {

std::terminate_handler previous_handler = nullptr;
std::atomic<bool> installed{false};

// Terminating while reporting a termination would recurse until the stack
// runs out, which is a worse ending than the one we are already having. The
// first thread in reports; any other just proceeds to the chained handler.
std::atomic_flag reporting = ATOMIC_FLAG_INIT;

std::string current_thread_id() {
  try {
    std::ostringstream ss;
    ss << boost::this_thread::get_id();
    return ss.str();
  } catch (...) {
    return "<unknown>";
  }
}

void handle_terminate() {
  if (!reporting.test_and_set()) {
    try {
      nsclient::logging::logger_helper::log_fatal("FATAL: NSClient++ was terminated by an uncaught exception on thread " + current_thread_id() + ": " +
                                                  nsclient::describe_current_exception() +
                                                  ". This is a bug: every entry point and worker thread is supposed to catch. Please report it with the "
                                                  "surrounding nsclient.log at https://github.com/mickem/nscp/issues");
    } catch (...) {
      // Nothing left to try. Fall through to the chained handler.
    }
  }
  // Chain rather than replace: on Windows the CRT's own handler is what
  // produces the crash dump the breakpad hook and Windows Error Reporting
  // pick up, and swallowing it would trade a crash report for a log line
  // instead of adding one.
  if (previous_handler != nullptr && previous_handler != &handle_terminate) {
    previous_handler();
  }
  std::abort();
}

}  // namespace

std::string nsclient::describe_current_exception() {
  const std::exception_ptr active = std::current_exception();
  if (!active) {
    return "no active exception (terminate was called directly)";
  }
  try {
    std::rethrow_exception(active);
  } catch (const std::exception &e) {
    // The type name is mangled on gcc/clang and spelled out on MSVC. Neither
    // is pretty, and both beat guessing which of the hundred things in a
    // check produced "basic_string::at" - boost::bad_optional_access in
    // particular is far easier to act on when it is named (#1499).
    try {
      return std::string(typeid(e).name()) + ": " + e.what();
    } catch (...) {
      return "an exception derived from std::exception";
    }
  } catch (const std::string &s) {
    return "std::string: " + s;
  } catch (const char *s) {
    return std::string("const char *: ") + (s == nullptr ? "" : s);
  } catch (...) {
    return "an exception not derived from std::exception";
  }
}

void nsclient::install_fatal_handlers() {
  if (installed.exchange(true)) return;
  previous_handler = std::set_terminate(&handle_terminate);
}

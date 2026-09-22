// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "fatal_handler.hpp"

#include <atomic>
#include <boost/thread.hpp>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <nsclient/logger/logger_helper.hpp>
#include <sstream>
#include <string>
#include <thread>
#include <threads/guarded_thread.hpp>
#include <typeinfo>

namespace {

#ifdef _MSC_VER
// MSVC documents set_terminate() as *per-thread* state: the handler installed
// on the main thread does not cover a worker, so a worker that reached
// std::terminate() aborted with no report at all - exactly the silence this
// file exists to end, on the platform where the socket server pool makes it
// reachable. Every thread therefore installs its own and remembers its own
// predecessor (the CRT's, which is what produces the crash dump).
// threads::run_guarded() calls install_fatal_handlers() for every guarded
// thread through the hook registered below.
#define NSCP_TERMINATE_HANDLER_IS_PER_THREAD 1
thread_local std::terminate_handler previous_handler = nullptr;
thread_local bool installed = false;
#else
// libstdc++ / libc++ keep one handler for the process, so installing it once
// covers every thread and re-installing is a no-op.
std::terminate_handler previous_handler = nullptr;
std::atomic<bool> installed{false};
#endif

// Terminating while reporting a termination would recurse until the stack
// runs out, which is a worse ending than the one we are already having. The
// first thread in reports; see handle_terminate() for what the others do.
std::atomic_flag reporting = ATOMIC_FLAG_INIT;
std::atomic<bool> reported{false};

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
    reported.store(true);
  } else {
    // Another thread got there first and is writing the report right now.
    // Falling straight through to abort() would end the process mid-line and
    // truncate the one sentence that explains the crash, so wait for it
    // instead. Bounded, because the reporter can itself be stuck on a full
    // disk or a dead NFS mount and a hung agent is worse than a torn line;
    // std::this_thread::sleep_for rather than boost's, which is an
    // interruption point and could throw from inside a terminate handler.
    for (int i = 0; i < 200 && !reported.load(); ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(25));
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
#ifdef NSCP_TERMINATE_HANDLER_IS_PER_THREAD
  if (installed) return;
  installed = true;
#else
  if (installed.exchange(true)) return;
#endif
  previous_handler = std::set_terminate(&handle_terminate);
  // Cover the threads this binary starts, not just this one. On MSVC that is
  // the whole point (see the per-thread note above); elsewhere every call
  // after the first returns immediately.
  threads::set_thread_start_hook(&nsclient::install_fatal_handlers);
}

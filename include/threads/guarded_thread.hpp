// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <atomic>
#include <boost/thread.hpp>
#include <memory>
#include <string>
#include <threads/stop_requested.hpp>

/**
 * A thread body that lets an exception escape calls std::terminate(), which
 * takes the whole agent down: every check on the host goes stale because one
 * collector hit one bad sample. The DLL entry points in
 * nscapi/nscapi_plugin_wrapper.hpp have been individually wrapped for exactly
 * this reason since forever, but a worker thread is not an entry point -
 * nothing stands between its body and the runtime - so each thread had to
 * remember to guard itself, and several did not.
 *
 * start_guarded_thread() is the one way to start a background thread: the
 * guard comes with starting one rather than being something a body opts into.
 *
 * The reporter is an explicit argument rather than a global hook because the
 * core and each module log through a different object, and a process-global
 * would not be shared across the DLL boundary on Windows anyway. Modules pass
 * NSC_THREAD_REPORTER (nscapi/macros.hpp); everything else passes a lambda
 * over whatever logger it already holds.
 *
 * Reporter contract: report(message) writes one finished line. The wording is
 * rendered here, in render_thread_event(), and NOT by the reporter: every
 * reporter used to paste the prefix in by hand and two of them had drifted
 * ("Worker '...'", "Scheduler thread '...'") away from the string the upgrade
 * note tells operators to alert on. A reporter picks the log level and the
 * channel; it does not get to reword the event.
 *
 * What the guard does NOT do is restart the body. A thread that dies leaves
 * the agent up but no longer collecting, which is quieter than a crash and
 * not obviously better, so the report has to be loud. How loud is up to the
 * reporter: modules log it at critical (NSC_THREAD_REPORTER), while the core
 * reports through whatever logger the owning object already holds, which is
 * an error. The wording is the same either way, so that is what an operator
 * alerts on. Restart policy is per-worker and belongs in the body: see
 * fleet_sync::thread_proc() for a supervisor loop that retries with a
 * widening backoff.
 */
namespace threads {

namespace detail {

/**
 * The hook run once on every guarded thread before its body, or null.
 *
 * This exists for std::set_terminate(), which MSVC documents as *per-thread*
 * state: a terminate handler installed on the main thread does not cover a
 * worker, so on Windows a worker that reached std::terminate() aborted with
 * no report at all - precisely the silence the handler was added to end. The
 * fix is for each thread to install its own, which is what the core registers
 * here (see service/fatal_handler.hpp). On Linux the handler is process-wide
 * and re-installing is a cheap no-op.
 *
 * It is a hook rather than a direct call because the installer lives in the
 * service binary and this header is compiled into every module DLL, which
 * does not link it. Each module therefore gets its own copy of this pointer,
 * left null, and behaves exactly as it does today; the core sets its own copy
 * and covers every thread it starts - which is every thread the security
 * notice is about.
 */
inline std::atomic<void (*)()> &thread_start_hook() {
  static std::atomic<void (*)()> hook{nullptr};
  return hook;
}

inline void run_thread_start_hook() {
  void (*const hook)() = thread_start_hook().load(std::memory_order_acquire);
  if (hook == nullptr) return;
  try {
    hook();
  } catch (...) {
    // A hook that fails must never cost us the thread it was meant to
    // protect.
  }
}

/**
 * The one place a thread event is worded. See the reporter contract above.
 */
inline std::string render_thread_event(const std::string &name, const std::string &detail) { return "Thread '" + name + "': " + detail; }

template <typename Report>
void report_thread_event(const std::string &name, const std::string &detail, Report report) {
  try {
    report(render_thread_event(name, detail));
  } catch (...) {
    // The reporter is a logger, and a thread can outlive the thing it logs
    // through during an unclean shutdown. There is nothing left to report to,
    // and letting this escape would be precisely the terminate() the guard
    // exists to prevent.
  }
}

// The text of the exception currently being handled, for a report.
inline std::string what_of(const std::exception &e) { return e.what(); }
}  // namespace detail

/**
 * Register a function to run at the top of every guarded thread started from
 * this binary, before its body. Called once, from main(); see
 * detail::thread_start_hook() for what it is for and why it is not a direct
 * call.
 */
inline void set_thread_start_hook(void (*hook)()) { detail::thread_start_hook().store(hook, std::memory_order_release); }

/**
 * Run `body`, catching anything that escapes it.
 *
 * boost::thread_interrupted and threads::stop_requested are the normal
 * shutdown exits and return quietly; everything else is reported as the death
 * of a worker. `name` is what the operator sees in that report, so it names
 * the worker ("checkdisk collector"), not the function.
 */
template <typename Body, typename Report>
void run_guarded(const std::string &name, Body body, Report report) {
  detail::run_thread_start_hook();
  try {
    body();
  } catch (const boost::thread_interrupted &) {
    // Cooperative interruption - the normal exit path on shutdown.
  } catch (const stop_requested &) {
    // The worker let go of a stalled source because stop() fired. Also normal.
  } catch (const std::exception &e) {
    detail::report_thread_event(name, "terminated by an uncaught exception: " + detail::what_of(e), report);
  } catch (...) {
    detail::report_thread_event(name, "terminated by an uncaught exception of unknown type", report);
  }
}

/**
 * Start `body` on a new thread, guarded by run_guarded().
 *
 * Returns the thread so the caller can join it exactly as it would a
 * boost::thread it had created itself; the guard changes nothing about
 * ownership, interruption or joining.
 */
template <typename Body, typename Report>
std::shared_ptr<boost::thread> start_guarded_thread(const std::string &name, Body body, Report report) {
  return std::make_shared<boost::thread>([name, body, report]() mutable { run_guarded(name, body, report); });
}

}  // namespace threads

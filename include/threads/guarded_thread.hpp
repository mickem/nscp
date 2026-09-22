// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

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
 * Reporter contract: report(name, detail) renders one line naming the worker
 * and what happened to it, e.g. "Thread 'checkdisk collector': <detail>".
 * The detail is a whole clause, so a reporter must not add a verb of its own
 * - the same reporter is used where the thread died and where an event loop
 * recovered (threads/guarded_io_context.hpp).
 *
 * What the guard does NOT do is restart the body. A thread that dies leaves
 * the agent up but no longer collecting, which is quieter than a crash and
 * not obviously better, so the report goes out at critical. Restart policy is
 * per-worker and belongs in the body: see fleet_sync::thread_proc() for a
 * supervisor loop that retries with a widening backoff.
 */
namespace threads {

namespace detail {
template <typename Report>
void report_thread_event(const std::string &name, const std::string &detail, Report report) {
  try {
    report(name, detail);
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
 * Run `body`, catching anything that escapes it.
 *
 * boost::thread_interrupted and threads::stop_requested are the normal
 * shutdown exits and return quietly; everything else is reported as the death
 * of a worker. `name` is what the operator sees in that report, so it names
 * the worker ("checkdisk collector"), not the function.
 */
template <typename Body, typename Report>
void run_guarded(const std::string &name, Body body, Report report) {
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

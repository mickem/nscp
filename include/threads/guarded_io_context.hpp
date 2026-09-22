// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/asio/io_context.hpp>
#include <string>
#include <threads/guarded_thread.hpp>

/**
 * io_context::run() is the one place in the codebase where somebody else's
 * exception surfaces on our thread. A completion handler runs arbitrary code -
 * in the socket servers it reaches all the way into a check - and asio does
 * not catch for it: whatever the handler throws propagates straight out of
 * run(). On a bare pool thread that is std::terminate() and the agent is gone,
 * because one client sent one request the protocol did not like.
 *
 * asio explicitly supports calling run() again after a handler has thrown: the
 * io_context is still usable and the remaining handlers are still queued. So
 * the right response is not to let the pool thread die (which silently leaves
 * the server one worker short, and eventually with none) but to log what
 * escaped and re-enter the loop.
 *
 * Kept out of guarded_thread.hpp so that every module starting a worker does
 * not pull <boost/asio> in with it.
 */
namespace threads {

/**
 * Run `io` until it runs out of work, re-entering run() if a completion
 * handler let an exception escape. Returns only on a clean exit or a
 * shutdown; see threads::run_guarded for the reporter contract.
 */
template <typename Report>
void run_io_context_guarded(const std::string &name, boost::asio::io_context &io, Report report) {
  for (;;) {
    try {
      io.run();
      return;
    } catch (const boost::thread_interrupted &) {
      return;
    } catch (const stop_requested &) {
      return;
    } catch (const std::exception &e) {
      detail::report_thread_event(name, "a completion handler threw (" + detail::what_of(e) + "); restarting the event loop", report);
    } catch (...) {
      detail::report_thread_event(name, "a completion handler threw an exception of unknown type; restarting the event loop", report);
    }
  }
}

}  // namespace threads

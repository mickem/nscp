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
 *
 * What re-entering does and does not recover is worth being precise about,
 * because the report says so. Handlers still queued on the io_context are
 * unaffected and keep running - that is the recovery. Anything the throwing
 * handler itself owned is gone: for a coroutine (asio::spawn) that means the
 * coroutine, so a server whose accept loop lives in one does not get its
 * acceptor back by re-entering run(), and has to respawn it - see
 * ServerBeastImpl::spawn_accept_loop(). The message is therefore worded as
 * "the remaining handlers continue", which is true for every caller, rather
 * than as a restart of the loop, which was not.
 */
template <typename Report>
void run_io_context_guarded(const std::string &name, boost::asio::io_context &io, Report report) {
  detail::run_thread_start_hook();
  for (;;) {
    try {
      io.run();
      return;
    } catch (const boost::thread_interrupted &) {
      return;
    } catch (const stop_requested &) {
      return;
    } catch (const std::exception &e) {
      detail::report_thread_event(name, "a completion handler threw (" + detail::what_of(e) + "); the remaining handlers continue", report);
    } catch (...) {
      detail::report_thread_event(name, "a completion handler threw an exception of unknown type; the remaining handlers continue", report);
    }
  }
}

}  // namespace threads

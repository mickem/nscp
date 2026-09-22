// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <string>

/**
 * The backstop under everything else.
 *
 * Every boundary that can be wrapped is wrapped: the DLL entry points in
 * nscapi/nscapi_plugin_wrapper.hpp, nscp_main(), and since
 * threads/guarded_thread.hpp every worker thread. What is left is the paths
 * nobody thought of - and those used to kill the agent in complete silence.
 * An uncaught C++ exception ends in std::terminate() -> abort(), which on
 * Windows does not reliably reach the SEH filter breakpad installs
 * (that one is for access violations and friends) and on Linux reaches
 * nothing at all. The service simply vanished, with nothing in nsclient.log
 * and nothing saying what threw.
 *
 * install_fatal_handlers() closes that: whatever is left gets one line naming
 * the exception type, its what() and the thread, written through the
 * last-resort channel before the process goes down. It cannot keep the agent
 * alive - by the time terminate() runs, unwinding has already failed - but an
 * operator gets a report instead of a mystery, and the chained handler still
 * produces whatever crash dump it would have.
 */
namespace nsclient {

/**
 * One line describing the exception currently propagating, or the one most
 * recently caught on this thread: "<type>: <what()>" where it can be
 * determined, a plain description where it cannot.
 *
 * Separate from the handler so it can be tested - the handler itself ends in
 * abort() and cannot be.
 */
std::string describe_current_exception();

/**
 * Install the last-resort handlers. Idempotent, and meant to be called from
 * the first line of main(), before anything that could throw.
 *
 * It also registers itself as threads::set_thread_start_hook(), so every
 * guarded thread this binary starts installs the handler too. That matters on
 * Windows: MSVC documents std::set_terminate() as per-thread state, so a
 * handler installed only on the main thread leaves every worker - including
 * the network-facing socket server pool - terminating in the same silence
 * this file exists to end. On Linux the handler is process-wide and the extra
 * calls return immediately.
 */
void install_fatal_handlers();

}  // namespace nsclient

// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/log.hpp>
#include <string>

namespace nsclient {
namespace logging {

struct logger_helper {
  static std::string get_formated_date(const std::string &format);
  // The last-resort logging channel: straight to the console and appended to
  // a file, with no queue, no settings lookup and no lock in the way. It is
  // what the parts of the logger that cannot log through the logger use, and
  // what the terminate handler uses when the agent is already dying.
  static void log_fatal(std::string message);
  // Where log_fatal() appends. Defaults to "nsclient.fatal" in the working
  // directory, which for an installed service is wherever the SCM happened to
  // start it; the core repoints it at the log folder once settings have been
  // read. Call it once, early, from one thread: the path is published through
  // an atomic and read without a lock, because log_fatal() can run from a
  // terminate handler where taking one risks deadlocking against a thread
  // that is already gone.
  //
  // The path is only adopted if a report can actually be written there: the
  // file is opened for append exactly as log_fatal() will open it, and if
  // that fails the same file name in the system temp folder is tried instead.
  // Returns the path now in use, which the caller should compare against what
  // it asked for and log when the two differ - a last-resort channel that
  // silently goes nowhere is worse than no channel at all.
  //
  // A folder that does not exist yet is not a failure and is deliberately not
  // created here: this runs on every start, including every short-lived
  // command line invocation, and the agent has nothing to report yet.
  // log_fatal() creates it at the moment it has a report to write.
  static std::string set_fatal_file(const std::string &path);
  // The path log_fatal() is currently appending to. Mostly for tests, which
  // have to put it back where they found it, and for anything that wants to
  // tell an operator where to look.
  static std::string fatal_file();
  // Where a report for `file` goes when `file` itself cannot be written: the
  // same file name in a directory of our own under the system temp folder,
  // created 0700 and only used if it really is ours. Empty when no such
  // directory can be had, in which case there is no fallback and the report
  // is dropped rather than written somewhere another account controls.
  static std::string fatal_fallback_file(const std::string &file);
  static std::pair<bool, std::string> render_console_message(bool oneline, const std::string &data);
  static std::string render_log_level_short(PB::Log::LogEntry::Entry::Level l);
  static std::string render_log_level_long(PB::Log::LogEntry::Entry::Level l);
};

}  // namespace logging
}  // namespace nsclient
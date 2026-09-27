// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Running a system tool from a check: systemctl on Linux, launchctl and
// softwareupdate on macOS. POSIX, shared by both platforms.

#include <cstddef>
#include <string>
#include <vector>

namespace system_exec {

struct exec_result {
  std::string output;          // everything the child wrote to stdout
  std::string error_output;    // bounded stderr, when requested
  std::string failure;         // which system call or limit failed
  bool started = false;        // the child was forked and exec'd
  bool timed_out = false;      // killed at the deadline; output is what it wrote so far
  bool output_failed = false;  // read failure or the caller's output limit was exceeded
  bool output_limit_exceeded = false;
  int error_number = 0;  // errno from setup, exec, poll, read or wait
  int exit_code = -1;    // the exit status, or -1 when it did not exit normally
};

// Execute a program directly (no shell) and capture stdout; stderr goes to
// /dev/null unless capture_stderr is set (then capture at most 4 KiB, draining
// the rest). argv[0] is the program - an absolute path, or a name looked up
// on PATH - and the remaining elements are passed verbatim. The child is
// killed once `timeout_ms` has passed, measured as one deadline for the whole
// run rather than per read - including the wait for it to exit after it
// closed its stdout. A nonzero max_output also bounds captured bytes; exceeding
// it kills the child and sets output_failed. Zero means no output limit.
exec_result run(const std::vector<std::string> &argv, int timeout_ms = 30000, std::size_t max_output = 0, bool capture_stderr = false);

// Inventory must not treat a failed command or partial output as an empty
// successful snapshot. Bound its output as well as its runtime, and throw.
std::string run_inventory_command(const std::vector<std::string> &argv, int timeout_ms = 30000, std::size_t max_output = 4 * 1024 * 1024);

// run() for callers that only want the output: empty when the program could
// not be started.
std::string exec_command(const std::vector<std::string> &argv, int timeout_ms = 30000);

}  // namespace system_exec

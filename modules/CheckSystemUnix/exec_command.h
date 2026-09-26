// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Running a system tool from a check: systemctl on Linux, launchctl and
// softwareupdate on macOS. POSIX, shared by both platforms.

#include <string>
#include <vector>

namespace system_exec {

struct exec_result {
  std::string output;     // everything the child wrote to stdout
  bool started = false;   // the child was forked and exec'd
  bool timed_out = false; // killed at the deadline; output is what it wrote so far
  int exit_code = -1;     // the exit status, or -1 when it did not exit normally
};

// Execute a program directly (no shell) and capture stdout; stderr goes to
// /dev/null. argv[0] is the program - an absolute path, or a name looked up
// on PATH - and the remaining elements are passed verbatim. The child is
// killed once `timeout_ms` has passed, measured as one deadline for the whole
// run rather than per read.
exec_result run(const std::vector<std::string> &argv, int timeout_ms = 30000);

// run() for callers that only want the output: empty when the program could
// not be started.
std::string exec_command(const std::vector<std::string> &argv, int timeout_ms = 30000);

}  // namespace system_exec

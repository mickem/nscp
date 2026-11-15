// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Linux reader for the systemd diagnostics sections: systemctl and journalctl,
// run directly (no shell), each bounded in time and output. The build links no
// libsystemd, so the tools are the interface; they run on the diagnostics
// thread only, every few seconds, never from fetchDiagnostics.

#include <sys/stat.h>
#include <time.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "diagnostics_thread.hpp"
#include "exec_command.h"

namespace systemd_diagnostics {
namespace source {

namespace {
const int timeout_ms = 10000;
const std::size_t max_output = 8 * 1024 * 1024;

std::string run_tool(const std::string &tool, const std::vector<std::string> &args) {
  std::vector<std::string> argv = {tool};
  argv.insert(argv.end(), args.begin(), args.end());
  const system_exec::exec_result r = system_exec::run(argv, timeout_ms, max_output, true);
  if (!r.started) throw std::runtime_error(tool + " could not be run" + (r.failure.empty() ? "" : ": " + r.failure));
  if (r.timed_out) throw std::runtime_error(tool + " did not finish within " + std::to_string(timeout_ms / 1000) + "s");
  if (r.output_failed) throw std::runtime_error(tool + ": " + (r.output_limit_exceeded ? "output too large" : "could not read its output"));
  if (r.exit_code != 0) {
    std::string why = r.error_output.substr(0, r.error_output.find('\n'));
    throw std::runtime_error(tool + " exited with " + std::to_string(r.exit_code) + (why.empty() ? "" : ": " + why));
  }
  return r.output;
}
}  // namespace

bool has_systemd() {
  struct stat st{};
  return ::lstat("/run/systemd/system", &st) == 0 && S_ISDIR(st.st_mode);
}

std::string systemctl(const std::vector<std::string> &args) { return run_tool("systemctl", args); }

std::string journalctl(const std::vector<std::string> &args) { return run_tool("journalctl", args); }

std::int64_t monotonic_now_us() {
  struct timespec ts{};
  if (::clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
  return static_cast<std::int64_t>(ts.tv_sec) * 1000000 + ts.tv_nsec / 1000;
}

}  // namespace source
}  // namespace systemd_diagnostics

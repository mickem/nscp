// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <algorithm>
#include <cstdint>
#include <nscapi/nscapi_diagnostics_helper.hpp>
#include <string>
#include <vector>

// The `services.problems` diagnostics section, shared by CheckSystem
// (Windows: the service control manager) and CheckSystemUnix (Linux: systemd).
//
// Producers are not told which service a failed check was about, so the
// section lists every service that is not where it should be: one meant to
// run that is not running, one that exited with an error, one that keeps
// being restarted. The module's collector refreshes it on a slow cadence (see
// refresh_seconds) and fetchDiagnostics copies the last result with
// add_section(); it never asks the service manager itself.
//
// No command lines: a service is its binary path, never its arguments.
namespace service_tables {

static constexpr const char *section_problems = "services.problems";

// How often a collector re-reads the service manager while the section is on.
static constexpr int refresh_seconds = 30;

struct service_row {
  // The service's own name (`acme-sync`; a systemd unit without `.service`).
  std::string name;
  // Its state as the platform says it: stopped, running, start_pending
  // (Windows); failed, activating (auto-restart) (Linux).
  std::string state;
  // How it last exited, when known: the Win32 exit code or the service's own
  // code (Windows), the main process's exit status (Linux).
  bool has_exit_code = false;
  std::int64_t exit_code = 0;
  // The executable, without its arguments.
  std::string binary_path;
  // The account it runs as.
  std::string account;
  // When its current process started, seconds since the epoch; 0 when it has
  // none or it is not known.
  std::int64_t started = 0;
  // How often it was restarted automatically (systemd NRestarts). Windows
  // keeps no such count.
  bool has_restart_count = false;
  std::int64_t restart_count = 0;
};

struct sample {
  // 0 until the first refresh.
  std::int64_t captured_at = 0;
  std::vector<service_row> rows;
  // More services were in trouble than rows were kept.
  bool truncated = false;
  // Set when the service manager could not be read: the section then says so
  // rather than reporting a healthy machine.
  std::string error;
};

namespace detail {
inline bool restarting_first(const service_row &a, const service_row &b) {
  const std::int64_t ra = a.has_restart_count ? a.restart_count : 0;
  const std::int64_t rb = b.has_restart_count ? b.restart_count : 0;
  if (ra != rb) return ra > rb;
  const bool ea = a.has_exit_code && a.exit_code != 0;
  const bool eb = b.has_exit_code && b.exit_code != 0;
  if (ea != eb) return ea;
  return a.name < b.name;
}
}  // namespace detail

// Order the rows for the table and keep at most `keep`: the core's (lower) row
// cap cuts from the end, so what a reader sees first is what most likely
// explains a failure - the services restarted most often, then those that
// exited with an error, then the rest by name.
inline sample make_sample(std::vector<service_row> rows, const std::int64_t now, const std::size_t keep = nscapi::diagnostics::max_rows) {
  sample out;
  out.captured_at = now;
  std::stable_sort(rows.begin(), rows.end(), &detail::restarting_first);
  if (rows.size() > keep) {
    rows.resize(keep);
    out.truncated = true;
  }
  out.rows = std::move(rows);
  return out;
}

// A refresh that failed: the section reports the error from now on.
inline sample failed_sample(const std::string &error, const std::int64_t now) {
  sample out;
  out.captured_at = now;
  out.error = error;
  return out;
}

// The section from the last refresh; nothing before the first one.
inline void add_section(PB::Diagnostics::DiagnosticsMessage *out, const sample &s) {
  using nscapi::diagnostics::type_int;
  using nscapi::diagnostics::type_string;
  using nscapi::diagnostics::type_timestamp;
  if (s.captured_at == 0) return;
  auto t = nscapi::diagnostics::table(out, section_problems)
               .describe("Services that are not where they should be: meant to run and not running, exited with an error, or restarting")
               .captured_at(s.captured_at);
  t.column("name")
      .column("state")
      .column("exit_code", type_int, "", "How the service last exited, when known")
      .column("binary_path", type_string, "", "The executable, without its arguments")
      .column("account", type_string, "", "The account the service runs as")
      .column("started", type_timestamp, "", "When the service's current process started")
      .column("restart_count", type_int, "", "Automatic restarts (systemd); null where the platform keeps no count");
  if (!s.error.empty()) {
    t.error(s.error);
    return;
  }
  if (s.truncated) t.truncated();
  for (const service_row &r : s.rows) {
    auto row = t.row();
    row.add(r.name).add(r.state);
    if (r.has_exit_code)
      row.add(r.exit_code);
    else
      row.add_null();
    row.add(r.binary_path).add(r.account);
    if (r.started > 0)
      row.add_time(r.started);
    else
      row.add_null();
    if (r.has_restart_count)
      row.add(r.restart_count);
    else
      row.add_null();
  }
}

}  // namespace service_tables

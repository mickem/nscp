// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// The pure halves of the systemd diagnostics sections - services.problems
// from `systemctl`, services.events and eventlog.errors from `journalctl` -
// exposed for unit tests. The readers that run those tools are in
// diagnostics_source_<platform>.cpp, the thread that calls them in
// diagnostics_thread.cpp.

#include <cstdint>
#include <diagnostics/event_tables.hpp>
#include <diagnostics/service_tables.hpp>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace systemd_diagnostics {

// ---- services.problems ------------------------------------------------------

// One row of `systemctl list-units --type=service --all --no-legend --plain`.
struct unit_listing {
  std::string id;  // acme-sync.service
  std::string load;
  std::string active;
  std::string sub;
};
std::vector<unit_listing> parse_list_units(const std::string &output);

// Whether a unit could be in trouble, from its listing alone: failed,
// waiting to be restarted, or running (it may have been restarted before -
// only its properties say). Everything else is left unread.
bool is_candidate(const unit_listing &unit);
// Whether the listing shows the unit up and running.
bool is_running(const unit_listing &unit);

// The units whose properties one refresh reads. A unit the listing shows in
// trouble is read every time. A running one only has something to say when
// it was restarted before, which only its properties tell - and a host has
// far more running units than troubled ones - so those are all read on a
// `sweep` and, in between, only the ones in `restarted`: the running units
// the last sweep found a restart count on.
std::vector<std::string> select_units(const std::vector<unit_listing> &units, bool sweep, const std::set<std::string> &restarted);

// The properties services.problems reads, for `systemctl show --property=`.
std::string show_properties();

// `systemctl show` output: one block of Key=Value lines per unit, separated
// by a blank line.
std::vector<std::map<std::string, std::string>> parse_show(const std::string &output);

// The executable of an ExecStart= value ({ path=/x ; argv[]=/x -a ; ... }):
// the path, never the argv. Empty when there is none.
std::string exec_path(const std::string &exec_start);

// A services.problems row from a unit's properties, or false when the unit is
// fine: one is in trouble when it failed, is waiting to be restarted, was
// restarted automatically (NRestarts) or its last run did not succeed.
// `now` and `monotonic_now_us` turn its CLOCK_MONOTONIC start time into a
// timestamp.
bool problem_row(const std::map<std::string, std::string> &unit, std::int64_t now, std::int64_t monotonic_now_us, service_tables::service_row &out);

// ---- the journal ------------------------------------------------------------

// The systemd manager's messages about a unit that failed, whose process
// exited, or that is being restarted (by MESSAGE_ID, from PID 1).
static constexpr const char *message_unit_failed = "be02cf6855d2428ba40df7e9d022f03d";          // a start job failed
static constexpr const char *message_unit_failure_result = "d9b373ed55a64feb8242e02dbe79a49c";  // "Failed with result"
static constexpr const char *message_unit_process_exit = "98e322203f7a4ed290d09fe03c09fe15";    // "Main process exited"
static constexpr const char *message_unit_restart = "5eb03494b6584870a536b337290809b3";         // "Scheduled restart job"

enum class feed { service_events, errors };

// The journalctl arguments for one read of a feed: JSON, newest `limit`
// entries, after `cursor` when there is one, else since `since` (seconds since
// the epoch).
std::vector<std::string> journal_args(feed which, const std::string &cursor, std::int64_t since, std::size_t limit);

struct journal_read {
  // Oldest first.
  std::vector<event_tables::event_row> rows;
  // The cursor of the last entry read (whether or not it became a row): the
  // next read starts after it.
  std::string cursor;
};

// `journalctl -o json` output, one object per line. A line that is not an
// object is skipped; a field that is not a string (binary data) is treated as
// missing.
journal_read parse_journal(feed which, const std::string &output);

}  // namespace systemd_diagnostics

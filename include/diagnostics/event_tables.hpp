// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <algorithm>
#include <cstdint>
#include <deque>
#include <mutex>
#include <nscapi/nscapi_diagnostics_helper.hpp>
#include <string>
#include <utility>
#include <vector>

// The two event diagnostics sections, `services.events` and
// `eventlog.errors`, shared by CheckEventLog (Windows: event log
// subscriptions) and CheckSystemUnix (Linux: the systemd journal).
//
// The platform half is a reader on the module's own thread: it renders each
// event once, as it arrives - the message text is the expensive, localised
// part - and pushes it into a ring. fetchDiagnostics then only copies the ring
// into the response with add_service_events() / add_errors(); it never reads
// a log itself.
namespace event_tables {

static constexpr const char *section_service_events = "services.events";
static constexpr const char *section_errors = "eventlog.errors";

// How far back a reader looks when it starts, so a failure shortly after the
// agent (or a settings reload) started still has the events that led to it.
static constexpr std::int64_t seed_seconds = 600;

struct event_row {
  // When the event was written, seconds since the unix epoch.
  std::int64_t time = 0;
  // Where it was written: the event log channel (Windows); the systemd unit
  // that wrote it, `kernel` or `journal` (Linux).
  std::string log;
  // Who wrote it: the event provider (Windows), the syslog identifier (Linux).
  std::string source;
  // The service the event is about (services.events only).
  std::string service;
  // The event id. Windows events have one; journal entries do not.
  bool has_id = false;
  std::int64_t id = 0;
  // critical, error (Windows); emerg, alert, crit, err (Linux).
  std::string level;
  // The rendered message, on one line, clipped and UTF-8 (see message()).
  std::string message;
  // What makes the event unique within its log (the channel and record id, a
  // journal cursor), so the backward query a reader seeds the ring with and
  // the live feed can overlap without an event showing twice. Never shown.
  std::string key;
};

// A message as a table cell: on one line (the reader sees a table), with runs
// of whitespace collapsed, and clipped to the cell cap on a UTF-8 boundary so
// the ring never holds more than a cell can carry.
inline std::string message(const std::string &text) {
  std::string out;
  out.reserve((std::min)(text.size(), nscapi::diagnostics::max_cell_length + 1));
  bool space = false;
  for (const char c : text) {
    if (c == '\r' || c == '\n' || c == '\t' || c == ' ') {
      space = !out.empty();
      continue;
    }
    if (space) out.push_back(' ');
    space = false;
    out.push_back(c);
    // Enough to know it will be clipped; the rest is never shown.
    if (out.size() > nscapi::diagnostics::max_cell_length + 4) break;
  }
  return nscapi::diagnostics::clip(out, nscapi::diagnostics::max_cell_length);
}

// The last `capacity` events, ordered by time, oldest dropped first. Pushed
// to by the module's reader thread and copied by fetchDiagnostics, so it
// carries its own lock - a small one, held only to insert or copy.
class ring {
 public:
  explicit ring(const std::size_t capacity = nscapi::diagnostics::max_rows) : capacity_(capacity == 0 ? 1 : capacity) {}
  ring(const ring &) = delete;
  ring &operator=(const ring &) = delete;

  // Insert by time (a seed and the live feed can interleave). False, and
  // nothing held, when an event with the same non-empty key is held already.
  bool push(event_row row) {
    const std::lock_guard<std::mutex> guard(mutex_);
    if (!row.key.empty()) {
      for (const event_row &r : rows_) {
        if (r.key == row.key) return false;
      }
    }
    ++seen_;
    // After every row written at the same second or earlier: equal times keep
    // the order they arrived in.
    auto at = rows_.end();
    while (at != rows_.begin() && std::prev(at)->time > row.time) --at;
    rows_.insert(at, std::move(row));
    while (rows_.size() > capacity_) {
      rows_.pop_front();
      ++dropped_;
    }
    return true;
  }

  struct snapshot {
    // Newest first.
    std::vector<event_row> rows;
    // Events pushed since the ring was made, and how many of them were pushed
    // out again by newer ones: a ring that dropped some holds "at least" its
    // rows.
    std::uint64_t seen = 0;
    std::uint64_t dropped = 0;
  };

  // Events a reader saw and chose not to keep - a burst it cut to the newest
  // ones before rendering them - so the ring still says it holds "at least".
  void count_dropped(const std::uint64_t n) {
    const std::lock_guard<std::mutex> guard(mutex_);
    seen_ += n;
    dropped_ += n;
  }

  snapshot copy() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    snapshot s;
    s.rows.assign(rows_.rbegin(), rows_.rend());
    s.seen = seen_;
    s.dropped = dropped_;
    return s;
  }

  std::size_t size() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return rows_.size();
  }

 private:
  mutable std::mutex mutex_;
  std::deque<event_row> rows_;
  std::size_t capacity_;
  std::uint64_t seen_ = 0;
  std::uint64_t dropped_ = 0;
};

// How a reader is doing: when it last confirmed the ring is current (0 for
// "now", as for a live subscription), and why it cannot read, if it cannot.
struct reader_state {
  std::int64_t captured_at = 0;
  std::string error;
};

namespace detail {
inline void finish(nscapi::diagnostics::table_builder &t, const ring::snapshot &s, const reader_state &state) {
  if (state.captured_at != 0) t.captured_at(state.captured_at);
  if (s.dropped > 0) t.truncated();
}
}  // namespace detail

// services.events: time, service, id, message - newest first.
inline void add_service_events(PB::Diagnostics::DiagnosticsMessage *out, const ring::snapshot &s, const reader_state &state) {
  using nscapi::diagnostics::type_int;
  using nscapi::diagnostics::type_timestamp;
  auto t = nscapi::diagnostics::table(out, section_service_events).describe("Services that crashed, failed or were restarted, newest first");
  t.column("time", type_timestamp)
      .column("service", nscapi::diagnostics::type_string, "", "The service the event is about")
      .column("id", type_int, "", "The event id (Windows); null for a journal entry")
      .column("message");
  detail::finish(t, s, state);
  if (!state.error.empty()) {
    t.error(state.error);
    return;
  }
  for (const event_row &e : s.rows) {
    auto r = t.row();
    r.add_time(e.time).add(e.service);
    if (e.has_id)
      r.add(e.id);
    else
      r.add_null();
    r.add(e.message);
  }
}

// eventlog.errors: time, log, source, id, level, message - newest first.
inline void add_errors(PB::Diagnostics::DiagnosticsMessage *out, const ring::snapshot &s, const reader_state &state) {
  using nscapi::diagnostics::type_int;
  using nscapi::diagnostics::type_timestamp;
  auto t = nscapi::diagnostics::table(out, section_errors).describe("The last errors written to the system logs, newest first");
  t.column("time", type_timestamp)
      .column("log")
      .column("source")
      .column("id", type_int, "", "The event id (Windows); null for a journal entry")
      .column("level")
      .column("message");
  detail::finish(t, s, state);
  if (!state.error.empty()) {
    t.error(state.error);
    return;
  }
  for (const event_row &e : s.rows) {
    auto r = t.row();
    r.add_time(e.time).add(e.log).add(e.source);
    if (e.has_id)
      r.add(e.id);
    else
      r.add_null();
    r.add(e.level).add(e.message);
  }
}

}  // namespace event_tables

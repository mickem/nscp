// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <algorithm>
#include <boost/optional.hpp>
#include <cstdint>
#include <string>

namespace onboarding {

// The header a fleet server sets on its desired-state answers to say it takes
// failure-context reports, lower-case as the http client stores headers:
// `X-Failure-Context: accept`, optionally `accept; min-interval=3600`.
extern const char *const failure_context_header;

// What the header says: absent (none) when the server does not take reports,
// or the minimum interval it asks for (0 for none) when it does. Anything
// that does not start with `accept` reads as not taking them.
boost::optional<unsigned long> parse_failure_context_header(const std::string &value);

// When the fleet sync reports a failed check, with the metrics snapshot it was
// recorded with, to the fleet server: the whole decision, kept apart from the
// transport so every rule can be tested with a clock the test controls.
// fleet_sync tells it when a loop turn starts, what the server says and how
// each report went, and asks it before every report.
//
// Times are wall-clock seconds since the epoch, not a steady clock: the last
// report's time is persisted across restarts, and a steady clock does not
// survive one.
//
// The rules:
//
//  * Only to a server that asked. Until a poll answer carries the header,
//    nothing is sent; a poll answer without it (a downgrade, a server that
//    switched the feature off) is back to sending nothing. A 404 or 410 on the
//    report is a server that does not have the route after all - the same as
//    no header, until a poll says otherwise again.
//  * One report per host per interval, not per check: `check_cpu` failing and
//    then `check_drive_size` within the hour is one report. The interval is
//    the operator's (`failure context interval`, at least 5 seconds), raised
//    to the server's `min-interval` when that is longer. The loop only asks
//    once per poll, so in practice the poll interval is a floor too.
//  * A report goes when there is a failure the server has not heard of (an
//    index past the last one sent) and the interval since the last report has
//    passed. The caller picks which failure; the rest are summarised in it.
//  * The last report's time survives a restart, so an agent that crash-loops
//    does not send one report per start. A stored time further in the future
//    than one interval is a clock that stepped back, and is ignored.
//  * Trouble in a loop turn - an error answer to its poll or report - means no
//    report in that turn. A rate-limited report (429, 503) means none in the
//    next turn either, and a Retry-After on it holds every report until it has
//    passed.
//  * A report the server rejected, or one that never arrived (the connection
//    dropped), is retried 1 min later, then 2, doubling up to an hour - never
//    sooner than the interval would allow anyway. A report it refused as too
//    large (413) is not retried: that failure is dropped, and the next one is
//    the next candidate.
class failure_context_pacer {
 public:
  static constexpr unsigned long min_interval_seconds = 5;
  static constexpr unsigned long default_interval_seconds = 3600;

  void configure(const unsigned long interval_seconds) { interval_ = (std::max)(interval_seconds, min_interval_seconds); }

  // The time of the last report sent before this process started, as
  // persisted. Indices do not survive a restart (the failed-check log starts
  // over at 1), so only the time is restored.
  void restore(const std::int64_t last_sent_at, const std::int64_t now) {
    if (last_sent_at <= 0) return;
    if (last_sent_at > now + static_cast<std::int64_t>(interval())) return;
    last_sent_at_ = last_sent_at;
  }

  // The sync starts (again, after a restart): no turn is carried over.
  void start() {
    skip_this_turn_ = false;
    skip_next_turn_ = false;
  }
  void begin_turn() {
    skip_this_turn_ = skip_next_turn_;
    skip_next_turn_ = false;
  }
  void trouble_this_turn() { skip_this_turn_ = true; }
  void quiet_next_turn() { skip_next_turn_ = true; }

  // A poll answer carried the header: the server takes reports, at most one
  // per `min_interval` seconds (0 for no minimum of its own).
  void server_accepts(const unsigned long min_interval) {
    accepted_ = true;
    server_min_interval_ = min_interval;
  }
  // A poll answer without it, or a 404/410 on the report.
  void server_silent() {
    accepted_ = false;
    server_min_interval_ = 0;
  }
  bool server_accepts_reports() const { return accepted_; }

  // The interval in force: the operator's, or the server's minimum if longer.
  unsigned long interval() const { return (std::max)(interval_, server_min_interval_); }

  // Whether to report now, when the newest failure in the log has index
  // `newest_index` (0 when the log is empty).
  bool due(const unsigned long long newest_index, const std::int64_t now) const {
    if (skip_this_turn_ || !accepted_) return false;
    if (newest_index <= last_index_) return false;
    if (now < hold_until_ || now < retry_at_) return false;
    if (last_sent_at_ > 0 && now < last_sent_at_ + static_cast<std::int64_t>(interval())) return false;
    return true;
  }

  // The server took the report of everything up to `index`.
  void sent(const unsigned long long index, const std::int64_t now) {
    last_index_ = (std::max)(last_index_, index);
    last_sent_at_ = now;
    rejections_ = 0;
    retry_at_ = 0;
  }

  // The report did not land (an error status, a dropped connection): try the
  // same failures again after the next backoff step.
  void rejected(const std::int64_t now) {
    retry_at_ = now + step(rejections_);
    ++rejections_;
  }

  // The report was too large for the server, and will be every time: the
  // failures up to `index` are given up on rather than retried.
  void dropped(const unsigned long long index) { last_index_ = (std::max)(last_index_, index); }

  // A Retry-After: no report before `seconds` have passed. A shorter hold
  // never cuts a longer one short.
  void hold(const std::int64_t now, const unsigned long seconds) {
    if (seconds == 0) return;
    hold_until_ = (std::max)(hold_until_, now + static_cast<std::int64_t>(seconds));
  }

  // The highest failure index the server has heard of (or that was dropped);
  // everything after it is news. 0 before the first report.
  unsigned long long last_index() const { return last_index_; }
  // When the last report went, persisted across restarts; 0 for never.
  std::int64_t last_sent_at() const { return last_sent_at_; }
  // For tests and diagnostics: the earliest a report may go.
  std::int64_t next_at() const {
    std::int64_t at = (std::max)(hold_until_, retry_at_);
    if (last_sent_at_ > 0) at = (std::max)(at, last_sent_at_ + static_cast<std::int64_t>(interval()));
    return at;
  }

 private:
  // The wait after `taken` earlier steps: 1 min, doubling, capped at an hour.
  static std::int64_t step(const unsigned int taken) {
    const unsigned int shift = (std::min)(taken, 6u);
    return (std::min)(static_cast<std::int64_t>(60) << shift, static_cast<std::int64_t>(3600));
  }

  unsigned long interval_ = default_interval_seconds;
  unsigned long server_min_interval_ = 0;
  bool accepted_ = false;
  unsigned long long last_index_ = 0;
  std::int64_t last_sent_at_ = 0;
  std::int64_t hold_until_ = 0;
  std::int64_t retry_at_ = 0;
  unsigned int rejections_ = 0;
  bool skip_this_turn_ = false;
  bool skip_next_turn_ = false;
};

}  // namespace onboarding

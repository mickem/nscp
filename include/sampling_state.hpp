// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <string>

/**
 * What a collector-backed check can say about the buffer it answers from.
 *
 * An empty buffer means one of two different things: the collector has not
 * tried to sample yet (the agent just started - warm-up), or it has tried and
 * every attempt failed (an unreadable /proc, a broken counter). The first one
 * ends by itself; the second one does not, and must never be reported as a
 * warm-up, or `warmup-state=ok` turns a dead collector into a permanent OK.
 */
namespace sampling {

enum class readiness { ready, warming_up, failed };

struct status {
  readiness state = readiness::warming_up;
  // Why the source is not ready; empty when it is.
  std::string error;
  // Failed until just now: readable again, first sample on the next attempt.
  bool recovering = false;
};

// Kept by the collector next to the buffer it describes, and updated under the
// same lock as the buffer, so a reader never sees an attempt counted before
// its sample is stored.
struct tracker {
  // Consecutive failed attempts after which the samples still in the buffer
  // are no longer answered from. The buffers average by sample count, not by
  // wall clock, so without this a source that died an hour ago would keep
  // reporting the hour-old averages as current. A few ticks, so one transient
  // failure does not flip a check to UNKNOWN.
  static const long long stale_after = 5;

  long long attempts = 0;
  long long consecutive_failures = 0;
  std::string last_error;
  // A delta source (CPU load, network rates) has read its baseline again after
  // failing: readable, but its first sample is only taken on the next attempt.
  bool baseline_pending = false;

  void succeeded() {
    ++attempts;
    consecutive_failures = 0;
    last_error.clear();
    baseline_pending = false;
  }
  void failed(const std::string &error) {
    ++attempts;
    ++consecutive_failures;
    last_error = error.empty() ? "unknown error" : error;
    baseline_pending = false;
  }
  // A delta source read its baseline and has nothing to store yet. Before any
  // attempt that is still the warm-up; after failures it is not - the
  // collector has tried, so warmup-state must not apply - only the reason
  // changes.
  void baseline() {
    if (attempts == 0) return;
    ++attempts;
    consecutive_failures = 0;
    last_error.clear();
    baseline_pending = true;
  }

  // `has_data`: whether the buffer holds a sample to answer from.
  status get(const bool has_data) const {
    status s;
    if (consecutive_failures >= stale_after) {
      s.state = readiness::failed;
      s.error = last_error;
    } else if (has_data) {
      s.state = readiness::ready;
    } else if (attempts == 0) {
      s.state = readiness::warming_up;
    } else {
      s.state = readiness::failed;
      s.recovering = baseline_pending;
      s.error = last_error;
    }
    return s;
  }
};

}  // namespace sampling

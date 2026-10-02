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
  // Why the most recent attempt failed; empty when it did not, or when an
  // attempt succeeded but produced nothing (no interfaces to report).
  std::string error;
};

// Kept by the collector next to the buffer it describes, and updated under the
// same lock as the buffer, so a reader never sees an attempt counted before
// its sample is stored.
struct tracker {
  long long attempts = 0;
  std::string last_error;

  void succeeded() {
    ++attempts;
    last_error.clear();
  }
  void failed(const std::string &error) {
    ++attempts;
    last_error = error.empty() ? "unknown error" : error;
  }

  // `has_data`: whether the buffer holds a sample to answer from.
  status get(const bool has_data) const {
    status s;
    if (has_data) {
      s.state = readiness::ready;
    } else if (attempts == 0) {
      s.state = readiness::warming_up;
    } else {
      s.state = readiness::failed;
      s.error = last_error;
    }
    return s;
  }
};

}  // namespace sampling

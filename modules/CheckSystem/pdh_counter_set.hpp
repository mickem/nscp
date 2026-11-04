// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <list>
#include <string>
#include <win/pdh/pdh_interface.hpp>
#include <win/pdh/pdh_query.hpp>

namespace pdh_counters {

// When the collector next tries the predefined counters it could not add.
// Fast at first, for perflib finishing its registration right after boot,
// then a steady cadence for as long as a counter stays missing: a counter
// registered by a service that starts late, or is installed while the agent
// runs, is picked up whenever it appears instead of at the next restart
// (#634).
struct retry_schedule {
  // The fast attempts after the initial one; a counter still missing after
  // them is reported as an error.
  static constexpr unsigned fast_attempts = 6;
  static constexpr unsigned long long steady_delay_ms = 60000;
  // How long to wait before retry number `attempt` (0-based).
  static unsigned long long delay_ms(unsigned attempt);
};

// A configured counter that could not be added, and why.
struct unresolved_counter {
  PDH::pdh_object config;
  std::string error;
};

// A configured counter and the instance that collects it.
struct resolved_counter {
  PDH::pdh_object config;
  PDH::pdh_instance instance;
};

// The predefined counters that are not in the collector's query yet. Each is
// added to the open query on its own, so one that is missing no longer fails
// the others, and adding one later leaves the counters already collected -
// and their previous sample - alone.
//
// Not thread-safe: owned by the collector thread.
class counter_set {
 public:
  // Every counter starts unresolved.
  void configure(const std::list<PDH::pdh_object> &configs);
  // Tries every unresolved counter: creates its instance (expanding a
  // wildcard, so instances that appeared since are picked up) and adds it to
  // `query`, which must be open. Returns the ones added; the others stay
  // unresolved with the reason they failed, whatever it was.
  std::list<resolved_counter> retry(PDH::PDHQuery &query);

  const std::list<unresolved_counter> &unresolved() const { return unresolved_; }
  bool complete() const { return unresolved_.empty(); }

 private:
  std::list<unresolved_counter> unresolved_;
};

}  // namespace pdh_counters

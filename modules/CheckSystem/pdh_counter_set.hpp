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
  // them is reported as an error, once.
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

// A configured counter that was added, and the instance that collects it. The
// instance is kept across query rebuilds, so its history survives another
// counter being added later.
struct resolved_counter {
  PDH::pdh_object config;
  PDH::pdh_instance instance;
};

// The predefined counters, split into the ones that resolved and the ones that
// have not (yet). Each counter is probed in a query of its own, so one missing
// counter can no longer fail the query the others are collected in.
//
// Not thread-safe: owned by the collector thread.
class counter_set {
 public:
  struct probe_result {
    std::list<resolved_counter> resolved;
    std::list<unresolved_counter> unresolved;
  };

  // Every counter starts unresolved.
  void configure(const std::list<PDH::pdh_object> &configs);
  // Tries each unresolved counter in a throwaway query. Changes nothing: the
  // caller adopts the result with commit() once it may touch the shared state.
  probe_result probe() const;
  // Adopts a probe: its resolved counters join resolved(), its unresolved
  // ones replace unresolved().
  void commit(const probe_result &probe);
  // Puts every resolved counter back on the unresolved list, for when the
  // query they were installed in failed as a whole.
  void demote_all(const std::string &error);

  // Adds every resolved counter to `query` after dropping what it held. The
  // query is left closed; the caller opens it.
  void fill(PDH::PDHQuery &query) const;

  const std::list<resolved_counter> &resolved() const { return resolved_; }
  const std::list<unresolved_counter> &unresolved() const { return unresolved_; }
  bool complete() const { return unresolved_.empty(); }

 private:
  std::list<resolved_counter> resolved_;
  std::list<unresolved_counter> unresolved_;
};

}  // namespace pdh_counters

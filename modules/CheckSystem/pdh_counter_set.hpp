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

// The predefined counters, split into the ones in the collector's query and
// the ones that are not (yet). Each counter is added to the open query on its
// own, so one missing counter no longer fails the others, and adding one
// later leaves the counters already collected - and their previous sample -
// alone.
//
// Not thread-safe: owned by the collector thread.
class counter_set {
 public:
  // What prepare() made of the unresolved counters.
  struct candidates {
    // Instances created, to be added to the query.
    std::list<resolved_counter> created;
    // Not there yet (a wildcard over an object that is not registered, or
    // with no instances): retried.
    std::list<unresolved_counter> missing;
    // Rejected by the counter's own configuration: retrying cannot help.
    std::list<unresolved_counter> invalid;
  };
  // What adopt() did with them.
  struct adoption {
    std::list<resolved_counter> added;
    std::list<unresolved_counter> invalid;
  };

  // Every counter starts unresolved.
  void configure(const std::list<PDH::pdh_object> &configs);
  // Creates the instances of the unresolved counters, expanding wildcards.
  // Changes nothing, so it can run without the lock the query is used under.
  candidates prepare() const;
  // Adds each created instance to `query`, which must be open. The ones PDH
  // accepts become resolved; the ones it rejects because the object is not
  // registered (yet) stay unresolved, as do prepare()'s missing ones. The
  // invalid ones - prepare()'s, and the ones PDH rejects for good (no such
  // counter in the object, a malformed path) - are dropped and handed back
  // for the caller to report.
  adoption adopt(const candidates &c, PDH::PDHQuery &query);

  const std::list<resolved_counter> &resolved() const { return resolved_; }
  const std::list<unresolved_counter> &unresolved() const { return unresolved_; }
  bool complete() const { return unresolved_.empty(); }

 private:
  std::list<resolved_counter> resolved_;
  std::list<unresolved_counter> unresolved_;
};

}  // namespace pdh_counters

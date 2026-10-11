// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "pdh_counter_set.hpp"

#include <str/utf8.hpp>

namespace pdh_counters {

unsigned long long retry_schedule::delay_ms(const unsigned attempt) {
  static const unsigned long long fast[fast_attempts] = {1000, 2000, 4000, 8000, 16000, 30000};
  return attempt < fast_attempts ? fast[attempt] : steady_delay_ms;
}

void counter_set::configure(const std::list<PDH::pdh_object> &configs) {
  resolved_.clear();
  unresolved_.clear();
  for (const PDH::pdh_object &config : configs) unresolved_.push_back({config, ""});
}

counter_set::probe_result counter_set::probe() const {
  probe_result result;
  for (const unresolved_counter &counter : unresolved_) {
    try {
      // A wildcard counter is expanded here, so instances that appeared since
      // the previous attempt are picked up.
      PDH::pdh_instance instance = PDH::factory::create(counter.config);
      // PdhAddCounter is where a counter that does not exist fails, and the
      // shared query adds every counter in one go: try this one on its own.
      PDH::PDHQuery query;
      query.addCounter(instance);
      query.open();
      query.close();
      result.resolved.push_back({counter.config, instance});
    } catch (const std::exception &e) {
      result.unresolved.push_back({counter.config, utf8::utf8_from_native(e.what())});
    }
  }
  return result;
}

void counter_set::commit(const probe_result &probe) {
  resolved_.insert(resolved_.end(), probe.resolved.begin(), probe.resolved.end());
  unresolved_ = probe.unresolved;
}

void counter_set::demote_all(const std::string &error) {
  for (const resolved_counter &counter : resolved_) unresolved_.push_back({counter.config, error});
  resolved_.clear();
}

void counter_set::fill(PDH::PDHQuery &query) const {
  query.removeAllCounters();
  for (const resolved_counter &counter : resolved_) query.addCounter(counter.instance);
}

}  // namespace pdh_counters

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
  unresolved_.clear();
  for (const PDH::pdh_object &config : configs) unresolved_.push_back({config, ""});
}

std::list<resolved_counter> counter_set::retry(PDH::PDHQuery &query) {
  std::list<resolved_counter> added;
  std::list<unresolved_counter> still_missing;
  for (const unresolved_counter &counter : unresolved_) {
    try {
      PDH::pdh_instance instance = PDH::factory::create(counter.config);
      // PdhAddCounter is where a counter that does not exist fails.
      query.addCounter(instance);
      added.push_back({counter.config, instance});
    } catch (const std::exception &e) {
      still_missing.push_back({counter.config, utf8::utf8_from_native(e.what())});
    }
  }
  unresolved_.swap(still_missing);
  return added;
}

}  // namespace pdh_counters

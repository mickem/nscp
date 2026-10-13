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

counter_set::candidates counter_set::prepare() const {
  candidates result;
  for (const unresolved_counter &counter : unresolved_) {
    try {
      // A wildcard counter is expanded here, so instances that appeared since
      // the previous attempt are picked up.
      result.created.push_back({counter.config, PDH::factory::create(counter.config)});
    } catch (const PDH::pdh_transient_exception &e) {
      // A wildcard over an object that is not registered yet, or that has no
      // instances right now.
      result.missing.push_back({counter.config, utf8::utf8_from_native(e.what())});
    } catch (const std::exception &e) {
      result.invalid.push_back({counter.config, utf8::utf8_from_native(e.what())});
    }
  }
  return result;
}

counter_set::adoption counter_set::adopt(const candidates &c, PDH::PDHQuery &query) {
  adoption result;
  result.invalid = c.invalid;
  unresolved_ = c.missing;
  for (const resolved_counter &counter : c.created) {
    try {
      // PdhAddCounter is where a counter that does not exist fails.
      query.addCounter(counter.instance);
      resolved_.push_back(counter);
      result.added.push_back(counter);
    } catch (const PDH::pdh_exception &e) {
      // A counter the object does not have, or a malformed path, stays that
      // way; an object that is not registered yet may still turn up.
      const unresolved_counter failed{counter.config, utf8::utf8_from_native(e.what())};
      if (e.status().is_permanent_add_failure()) {
        result.invalid.push_back(failed);
      } else {
        unresolved_.push_back(failed);
      }
    } catch (const std::exception &e) {
      unresolved_.push_back({counter.config, utf8::utf8_from_native(e.what())});
    }
  }
  return result;
}

}  // namespace pdh_counters

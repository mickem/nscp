// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <atomic>
#include <boost/date_time.hpp>
#include <parsers/helpers.hpp>

namespace parsers {
namespace where {
namespace {
// Written by every parse and date check (reset()) and read by date keywords
// while other checks evaluate on the worker pool, so it has to be atomic: a
// plain long long here was a data race between concurrent checks.
std::atomic<long long> now{0};
}  // namespace

long long constants::get_now() { return now.load(std::memory_order_relaxed); }
namespace pt = boost::posix_time;
namespace gt = boost::gregorian;
namespace dt = boost::date_time;

inline std::time_t to_time_t_epoch(const pt::ptime t) {
  if (t == dt::neg_infin) return 0;
  if (t == dt::pos_infin) return LONG_MAX;
  constexpr pt::ptime start(gt::date(1970, 1, 1));
  return (t - start).total_seconds();
}

void constants::reset() { now.store(to_time_t_epoch(pt::second_clock::universal_time()), std::memory_order_relaxed); }
}  // namespace where
}  // namespace parsers
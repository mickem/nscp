// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <string>

namespace mssql_sampling {

// Both DMV samplers use one server-side delay and one elapsed time for every
// row. The queries supply their own snapshot tables and result projections.
inline std::string batch(const std::string &first_snapshot, const std::string &second_snapshot) {
  return "SET NOCOUNT ON; DECLARE @t0 datetime2 = SYSDATETIME(); " + first_snapshot +
         "; WAITFOR DELAY '00:00:01'; DECLARE @elapsed_ms int = DATEDIFF(millisecond, @t0, SYSDATETIME()); " + second_snapshot;
}

// DMV counters can reset between snapshots. A negative delta is unavailable,
// not a negative rate, and must not be folded into a wait category or ratio.
inline long long delta(long long value, long long previous) {
  if (previous < 0 || value < previous) return -1;
  return value - previous;
}

inline double per_second(double delta, long long elapsed_ms) {
  if (delta < 0 || elapsed_ms <= 0) return -1;
  return delta * 1000.0 / static_cast<double>(elapsed_ms);
}

inline double rate(long long value, bool has_previous, long long previous, long long elapsed_ms) {
  return has_previous ? per_second(static_cast<double>(delta(value, previous)), elapsed_ms) : -1;
}

}  // namespace mssql_sampling

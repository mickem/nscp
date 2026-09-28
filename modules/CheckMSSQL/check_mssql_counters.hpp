// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/command.hpp>
#include <string>
#include <vector>

#include "odbc_query.hpp"

namespace check_mssql_counters_command {

// Raw counter row: the second snapshot of sys.dm_os_performance_counters
// joined with the first (prev_value), plus the measured sampling window.
struct counter_row {
  std::string name;
  long long value = 0;
  bool has_prev = false;
  long long prev_value = 0;
  long long elapsed_ms = 0;
};

// The engine counters as exposed to the filter engine (one row per instance).
// All rates are per second over the sampling window; -1 = counter unavailable.
struct counters_info {
  double hit_ratio = -1;             // buffer cache hit ratio, percent
  long long page_life_expectancy = -1;  // seconds
  double batch_requests = -1;
  double compilations = -1;
  double recompilations = -1;
  double lazy_writes = -1;
  double deadlocks = -1;
  double lock_waits = -1;

  double get_hit_ratio() const { return hit_ratio; }
  long long get_page_life_expectancy() const { return page_life_expectancy; }
  double get_batch_requests() const { return batch_requests; }
  double get_compilations() const { return compilations; }
  double get_recompilations() const { return recompilations; }
  double get_lazy_writes() const { return lazy_writes; }
  double get_deadlocks() const { return deadlocks; }
  double get_lock_waits() const { return lock_waits; }

  std::string show() const { return "mssql"; }
};

// Pure: fold the sampled counter rows into the summary object. Per-second
// rates use (value - prev) / elapsed; the hit ratio prefers the delta of
// value/base over the sampling window (the lifetime ratio converges to ~100%
// and hides regressions) and falls back to the lifetime ratio when the base
// did not move.
counters_info build_counters(const std::vector<counter_row> &rows);

void check(const mssql_odbc::connection_info &defaults, const PB::Commands::QueryRequestMessage::Request &request,
           PB::Commands::QueryResponseMessage::Response *response);

}  // namespace check_mssql_counters_command

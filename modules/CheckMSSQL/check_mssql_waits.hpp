// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/command.hpp>
#include <string>
#include <vector>

#include "odbc_query.hpp"

namespace check_mssql_waits_command {

// Raw per-wait-type delta over the sampling window (only rows whose wait time
// increased are returned by the query).
struct wait_row {
  std::string wait_type;
  long long wait_ms = 0;    // wait time accumulated during the window
  long long signal_ms = 0;  // part of wait_ms spent runnable (waiting for CPU)
  long long elapsed_ms = 0;
};

// The instance summary as exposed to the filter engine (one row).
// Wait rates are ms of wait accumulated per second of wall clock.
struct waits_info {
  long long schedulers = 0;      // visible online schedulers
  long long runnable_tasks = 0;  // tasks with CPU work waiting for a scheduler
  long long work_queue = 0;      // tasks queued with no worker at all
  long long workers = 0;         // active workers
  double cpu_waits = 0;
  double io_waits = 0;
  double log_waits = 0;
  double lock_waits = 0;
  double latch_waits = 0;
  double memory_waits = 0;
  double network_waits = 0;
  double other_waits = 0;
  double total_waits = 0;
  double signal_wait_pct = -1;  // -1 when nothing waited during the window

  long long get_schedulers() const { return schedulers; }
  long long get_runnable_tasks() const { return runnable_tasks; }
  long long get_work_queue() const { return work_queue; }
  long long get_workers() const { return workers; }
  double get_cpu_waits() const { return cpu_waits; }
  double get_io_waits() const { return io_waits; }
  double get_log_waits() const { return log_waits; }
  double get_lock_waits() const { return lock_waits; }
  double get_latch_waits() const { return latch_waits; }
  double get_memory_waits() const { return memory_waits; }
  double get_network_waits() const { return network_waits; }
  double get_other_waits() const { return other_waits; }
  double get_total_waits() const { return total_waits; }
  double get_signal_wait_pct() const { return signal_wait_pct; }

  std::string show() const { return "waits"; }
};

// Pure: classify a wait type into one of the categories used by the check:
// benign (excluded), cpu, io, log, lock, latch, memory, network or other.
std::string categorize_wait(const std::string &wait_type);

// Pure: fold the per-wait-type deltas into per-category rates (ms of wait per
// second) and the signal-wait percentage; benign waits are excluded entirely.
// Scheduler fields are left at zero for the caller to fill in.
waits_info build_waits(const std::vector<wait_row> &rows);

void check(const mssql_odbc::connection_info &defaults, const PB::Commands::QueryRequestMessage::Request &request,
           PB::Commands::QueryResponseMessage::Response *response);

}  // namespace check_mssql_waits_command

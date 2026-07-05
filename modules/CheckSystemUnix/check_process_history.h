// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#ifndef NSCP_CHECK_PROCESS_HISTORY_H
#define NSCP_CHECK_PROCESS_HISTORY_H

#include <list>
#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/metrics.hpp>
#include <string>

class pdh_thread;

namespace process_history_check {

// A process (by executable name) that has been seen since NSClient++ started.
// Mirrors the Windows process_history_check::process_record so queries are
// portable. Requires `process history = true` so the collector tracks it.
struct process_record {
  std::string exe;
  long long first_seen;
  long long last_seen;
  long long times_seen;
  bool currently_running;

  process_record() : first_seen(0), last_seen(0), times_seen(0), currently_running(false) {}
  process_record(const std::string &name, long long now) : exe(name), first_seen(now), last_seen(now), times_seen(1), currently_running(true) {}
  process_record(const process_record &other) = default;
  process_record &operator=(const process_record &other) = default;

  std::string get_exe() const { return exe; }
  long long get_first_seen() const { return first_seen; }
  long long get_last_seen() const { return last_seen; }
  long long get_times_seen() const { return times_seen; }
  std::string get_currently_running() const { return currently_running ? "true" : "false"; }
  long long get_currently_running_i() const { return currently_running ? 1 : 0; }

  std::string get_first_seen_s() const;
  std::string get_last_seen_s() const;

  std::string show() const { return exe; }
};

typedef std::list<process_record> history_type;

void check_process_history(std::shared_ptr<pdh_thread> collector, const PB::Commands::QueryRequestMessage::Request &request,
                           PB::Commands::QueryResponseMessage::Response *response);
void check_process_history_new(std::shared_ptr<pdh_thread> collector, const PB::Commands::QueryRequestMessage::Request &request,
                               PB::Commands::QueryResponseMessage::Response *response);

void build_process_history_metrics(PB::Metrics::MetricsBundle *parent, const history_type &data);

}  // namespace process_history_check

#endif  // NSCP_CHECK_PROCESS_HISTORY_H

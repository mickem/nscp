// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread/shared_mutex.hpp>
#include <list>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/metrics.hpp>
#include <set>
#include <string>

namespace process_history_check {

/**
 * Represents a process that has been seen since NSClient++ started.
 *
 * This tracks the history of all processes that have been running at any point,
 * allowing checks to verify if certain applications have been started or not.
 */
struct process_record {
  std::string exe;         // Executable name (e.g., "notepad.exe")
  long long first_seen;    // Unix timestamp when first seen
  long long last_seen;     // Unix timestamp when last seen
  long long times_seen;    // Number of times the process was observed running
  bool currently_running;  // Whether the process is currently running

  process_record() : first_seen(0), last_seen(0), times_seen(0), currently_running(false) {}
  process_record(const std::string &name, long long now) : exe(name), first_seen(now), last_seen(now), times_seen(1), currently_running(true) {}

  process_record(const process_record &other) = default;
  process_record &operator=(const process_record &other) = default;

  void build_metrics(PB::Metrics::MetricsBundle *section) const;

  std::string get_exe() const { return exe; }
  long long get_first_seen() const { return first_seen; }
  long long get_last_seen() const { return last_seen; }
  long long get_times_seen() const { return times_seen; }
  std::string get_currently_running() const { return currently_running ? "true" : "false"; }
  long long get_currently_running_i() const { return currently_running ? 1 : 0; }

  std::string get_first_seen_s() const;
  std::string get_last_seen_s() const;

  std::string show() const;
};

typedef std::list<process_record> history_type;

class process_history_data final {
  boost::shared_mutex mutex_;
  bool fetch_history_;
  std::map<std::string, process_record> history_;  // keyed by lowercase exe name

 public:
  process_history_data() : fetch_history_(true) {}

  void fetch();
  void fetch(const std::set<std::string> &running_exes);
  history_type get();
  void clear();
  long long get_count();

  void build_metrics(PB::Metrics::MetricsBundle *section);
};

namespace check {
void check_process_history(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                           history_type data);
void check_process_history_new(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                               const history_type &data);
}  // namespace check

}  // namespace process_history_check

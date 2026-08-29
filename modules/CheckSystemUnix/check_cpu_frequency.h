// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <list>
#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/metrics.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>

namespace cpu_frequency_check {

struct cpu_frequency {
  std::string name;  // e.g. "cpu0", "cpu1", or "total" for an aggregate
  long long current_mhz;
  long long max_mhz;
  long long min_mhz;

  cpu_frequency() : current_mhz(0), max_mhz(0), min_mhz(0) {}

  std::string get_name() const { return name; }
  long long get_current_mhz() const { return current_mhz; }
  long long get_max_mhz() const { return max_mhz; }
  long long get_min_mhz() const { return min_mhz; }
  long long get_frequency_pct() const { return max_mhz == 0 ? 0 : (current_mhz * 100 / max_mhz); }

  std::string show() const { return name; }
};

typedef std::list<cpu_frequency> cpus_type;

cpus_type read_cpu_frequency();
// Same, but reading from an alternate sysfs cpu directory (the layout of
// /sys/devices/system/cpu). Exposed for unit testing against fixture trees.
cpus_type read_cpu_frequency(const std::string &base_path);

void check_cpu_frequency(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
// Evaluate a pre-read list of cores against the request's filter/thresholds.
// Exposed for unit testing.
void check_cpu_frequency_evaluate(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                                  const cpus_type &data);

void build_cpu_frequency_metrics(PB::Metrics::MetricsBundle *parent);
void build_cpu_frequency_metrics(PB::Metrics::MetricsBundle *parent, const cpus_type &data);

}  // namespace cpu_frequency_check

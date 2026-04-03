/*
 * Copyright (C) 2004-2026 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <boost/thread/shared_mutex.hpp>
#include <list>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/metrics.hpp>
#include <string>
#include <win/wmi/wmi_query.hpp>

namespace cpu_frequency_check {

struct helper {
  static std::string query;
  static std::string ns;
};

struct cpu_frequency {
  std::string name;
  long long current_mhz;
  long long max_mhz;
  long long number_of_cores;
  long long number_of_logical_processors;

  cpu_frequency() : current_mhz(0), max_mhz(0), number_of_cores(0), number_of_logical_processors(0) {}
  cpu_frequency(const cpu_frequency &other) = default;
  cpu_frequency &operator=(const cpu_frequency &other) = default;

  void read_wmi(const wmi_impl::row &r);
  void build_metrics(PB::Metrics::MetricsBundle *section) const;

  std::string get_name() const { return name; }
  long long get_current_mhz() const { return current_mhz; }
  long long get_max_mhz() const { return max_mhz; }
  long long get_number_of_cores() const { return number_of_cores; }
  long long get_number_of_logical_processors() const { return number_of_logical_processors; }
  long long get_frequency_pct() const { return max_mhz == 0 ? 0 : (current_mhz * 100 / max_mhz); }

  std::string show() const { return name; }
};

typedef std::list<cpu_frequency> cpus_type;

class cpu_frequency_data {
  boost::shared_mutex mutex_;
  bool fetch_cpu_frequency_;
  cpus_type cpus_;

 public:
  cpu_frequency_data() : fetch_cpu_frequency_(true) {}

  void fetch();
  cpus_type get();

 private:
  static cpus_type query_wmi();
};

namespace check {
void check_cpu_frequency(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                         const cpus_type &data);
}  // namespace check

}  // namespace cpu_frequency_check

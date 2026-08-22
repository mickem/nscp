// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/optional.hpp>
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
  // Win32_Processor returns one instance per physical CPU socket, so socket_id
  // (DeviceID, e.g. "CPU0") and socket (SocketDesignation, e.g. "CPU 1") give a
  // per-socket dimension without needing the fragile logical-core -> socket mapping
  // the load path would otherwise require.
  std::string socket_id;
  std::string socket;
  long long current_mhz;
  long long max_mhz;
  long long number_of_cores;
  long long number_of_logical_processors;
  // WMI does not always have a LoadPercentage sample ready (the property is
  // NULL every now and then, #1391), so absence is a real state: none means
  // "no reading this cycle", never 0 (an idle CPU is a valid 0).
  boost::optional<long long> load_pct;
  // Inventory columns: cache sizes in bytes (0 when the platform does not
  // report them — common on VMs) and the architecture mapped to a name.
  long long l2_cache;
  long long l3_cache;
  std::string architecture;

  cpu_frequency() : current_mhz(0), max_mhz(0), number_of_cores(0), number_of_logical_processors(0), l2_cache(0), l3_cache(0) {}
  cpu_frequency(const cpu_frequency &other) = default;
  cpu_frequency &operator=(const cpu_frequency &other) = default;

  void read_wmi(const wmi_impl::row &r);
  void build_metrics(PB::Metrics::MetricsBundle *section) const;

  std::string get_name() const { return name; }
  std::string get_socket_id() const { return socket_id; }
  std::string get_socket() const { return socket; }
  long long get_current_mhz() const { return current_mhz; }
  long long get_max_mhz() const { return max_mhz; }
  long long get_number_of_cores() const { return number_of_cores; }
  long long get_number_of_logical_processors() const { return number_of_logical_processors; }
  boost::optional<long long> get_load_pct() const { return load_pct; }
  long long get_frequency_pct() const { return max_mhz == 0 ? 0 : (current_mhz * 100 / max_mhz); }
  long long get_l2_cache() const { return l2_cache; }
  long long get_l3_cache() const { return l3_cache; }
  std::string get_l2_cache_human() const;
  std::string get_l3_cache_human() const;
  std::string get_architecture() const { return architecture; }

  std::string show() const { return name; }
};

// Map a Win32_Processor.Architecture value to a name (0=x86, 5=ARM, 6=ia64,
// 9=x64, 12=ARM64, ...). Pure, exposed for unit tests.
std::string architecture_to_string(long long architecture);

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

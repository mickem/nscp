// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <atomic>
#include <boost/thread/thread.hpp>
#include <boost/unordered_map.hpp>
#include <boost/variant.hpp>
#include <map>
#include <memory>
#include <nscapi/settings/proxy.hpp>
#include <rrd_buffer.hpp>
#include <win/pdh/pdh_interface.hpp>
#include <win/pdh/pdh_query.hpp>
#include <win/sysinfo/win_sysinfo.hpp>

#include "check_battery.hpp"
#include "check_cpu_frequency.hpp"
#include "check_network.hpp"
#include "check_os_updates.hpp"
#include "check_process_history.hpp"
#include "check_temperature.hpp"
#include "filter_config_object.hpp"

struct spi_container {
  long long handles;
  long long procs;
  long long threads;
  spi_container() : handles(0), procs(0), threads(0) {}
};

class pdh_thread {
 public:
  typedef boost::variant<std::string, long long, double> value_type;
  typedef boost::unordered_map<std::string, value_type> metrics_hash;

 private:
  typedef boost::unordered_map<std::string, PDH::pdh_instance> lookup_type;
  typedef std::list<std::string> error_list;

  std::shared_ptr<boost::thread> thread_;
  boost::shared_mutex mutex_;
  HANDLE stop_event_;
  int plugin_id;
  nscapi::core_wrapper *core_;

  metrics_hash metrics;

  std::list<PDH::pdh_object> configs_;
  std::list<PDH::pdh_instance> counters_;
  rrd_buffer<windows::system_info::cpu_load> cpu;
  lookup_type lookups_;
  network_check::network_data network;
  temperature_check::temperature_data temperature;
  cpu_frequency_check::cpu_frequency_data cpu_frequency;
  battery_check::battery_data battery;
  os_updates_check::os_updates_data os_updates;
  process_history_check::process_history_data process_history;

 public:
  bool read_core_load;
  bool use_pdh_for_cpu;
  bool process_history_enabled;
  int min_threshold_;
  std::string subsystem;
  std::string disable_;
  std::string default_buffer_size;

 public:
  pdh_thread(nscapi::core_wrapper *core, int plugin_id)
      : stop_event_(nullptr),
        plugin_id(plugin_id),
        core_(core),
        read_core_load(true),
        use_pdh_for_cpu(false),
        process_history_enabled(false),
        min_threshold_(10) {
    mutex_.lock();
  }
  void add_counter(const PDH::pdh_object &counter);

  std::map<std::string, double> get_value(std::string counter);
  std::map<std::string, double> get_average(std::string counter, long seconds);
  std::map<std::string, long long> get_int_value(std::string counter);
  std::map<std::string, windows::system_info::load_entry> get_cpu_load(long seconds);

  network_check::nics_type get_network();
  temperature_check::zones_type get_temperature();
  cpu_frequency_check::cpus_type get_cpu_frequency();
  battery_check::batteries_type get_battery();
  os_updates_check::os_updates_obj get_os_updates();
  process_history_check::history_type get_process_history();
  metrics_hash get_metrics();

  bool start();
  bool stop() const;
  void set_path(const std::string mem_path, const std::string cpu_path, const std::string proc_path, const std::string legacy_path);

  void add_realtime_mem_filter(std::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query);
  void add_realtime_cpu_filter(std::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query);
  void add_realtime_proc_filter(std::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query);
  void add_realtime_legacy_filter(std::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query);

  void add_samples(std::shared_ptr<nscapi::settings_proxy> settings);

  std::string to_string() const { return "pdh"; }
  void ensure_default(std::shared_ptr<nscapi::settings_proxy> proxy);

  typedef std::map<std::string, std::shared_ptr<std::atomic<long long>>> count_map;
  typedef std::map<std::string, long long> non_atomic_count_map;
  non_atomic_count_map get_realtime_filter_counts();

 private:
  static spi_container fetch_spi(error_list &errors);
  void write_metrics(const spi_container &handles, const windows::system_info::cpu_load &load, PDH::PDHQuery *pdh, error_list &errors);

  // Single attempt at resolving counters and opening the PDH query. Rebuilds
  // counters_ and lookups_ from configs_ each call so that wildcard expansion
  // re-runs (perflib may have finished registering since the previous attempt).
  // Returns true on success; false on any per-counter or query-open failure.
  // log_failures_as_errors selects the log level used for failures (true on
  // the final attempt so the user sees what went wrong, false during retries
  // to avoid spamming the log on transient boot races).
  bool try_setup_pdh_counters(PDH::PDHQuery &pdh, bool log_failures_as_errors);

  filters::mem::filter_config_handler mem_filters_;
  filters::cpu::filter_config_handler cpu_filters_;
  filters::proc::filter_config_handler proc_filters_;
  filters::legacy::filter_config_handler legacy_filters_;

  non_atomic_count_map realtime_filter_counts_;
  void thread_proc();
};

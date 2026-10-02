// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <atomic>
#include <boost/circular_buffer.hpp>
#include <boost/thread.hpp>
#include <boost/unordered_map.hpp>
#include <error/error.hpp>
#include <functional>
#include <map>
#include <memory>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/settings/proxy.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <rrd_buffer.hpp>
#include <sampling_state.hpp>
#include <string>
#include <vector>

#include <set>

#include "check_network.h"
#include "check_process_history.h"
#include "collector_source.h"
#include "filter_config_object.hpp"

// CPU load entry structure (similar to Windows version)
struct load_entry {
  double idle;
  double user;
  double kernel;
  int core;

  load_entry() : idle(0.0), user(0.0), kernel(0.0), core(-1) {}
  load_entry(double idle, double user, double kernel, int core = -1) : idle(idle), user(user), kernel(kernel), core(core) {}
  load_entry(const load_entry &obj) : idle(obj.idle), user(obj.user), kernel(obj.kernel), core(obj.core) {}

  load_entry &operator=(const load_entry &obj) {
    idle = obj.idle;
    user = obj.user;
    kernel = obj.kernel;
    core = obj.core;
    return *this;
  }

  void add(const load_entry &other) {
    idle += other.idle;
    user += other.user;
    kernel += other.kernel;
  }

  void normalize(double value) {
    if (value > 0) {
      idle /= value;
      user /= value;
      kernel /= value;
    }
  }
};

// Structure to hold CPU load for all cores
struct cpu_load {
  int cores;
  load_entry total;
  std::vector<load_entry> core;

  cpu_load() : cores(0) {}

  void add(const cpu_load &other) {
    total.add(other.total);
    if (core.empty()) {
      core = other.core;
    } else {
      for (size_t i = 0; i < core.size() && i < other.core.size(); ++i) {
        core[i].add(other.core[i]);
      }
    }
  }

  void normalize(double value) {
    total.normalize(value);
    for (auto &c : core) {
      c.normalize(value);
    }
  }
};

// Structure to hold memory information
struct memory_entry {
  unsigned long long total;
  unsigned long long free;

  memory_entry() : total(0), free(0) {}
  memory_entry(unsigned long long total, unsigned long long free) : total(total), free(free) {}
  memory_entry(const memory_entry &obj) : total(obj.total), free(obj.free) {}

  memory_entry &operator=(const memory_entry &obj) {
    total = obj.total;
    free = obj.free;
    return *this;
  }

  void add(const memory_entry &other) {
    total += other.total;
    free += other.free;
  }

  void normalize(double value) {
    if (value > 0) {
      total = static_cast<unsigned long long>(total / value);
      free = static_cast<unsigned long long>(free / value);
    }
  }

  unsigned long long get_used() const { return total > free ? total - free : 0; }
};

// Structure to hold all memory types
struct memory_info {
  memory_entry physical;
  memory_entry cached;
  memory_entry swap;

  memory_info() {}

  void add(const memory_info &other) {
    physical.add(other.physical);
    cached.add(other.cached);
    swap.add(other.swap);
  }

  void normalize(double value) {
    physical.normalize(value);
    cached.normalize(value);
    swap.normalize(value);
  }
};


class pdh_thread {
 private:
  std::shared_ptr<boost::thread> thread_;
  mutable boost::shared_mutex mutex_;
  // Set by stop() on the module thread, polled by the collector thread.
  std::atomic<bool> stop_requested_;

  nscapi::core_wrapper *core_;
  int plugin_id_;

  // CPU data collection
  rrd_buffer<cpu_load> cpu_buffer_;

  // Memory data collection
  rrd_buffer<memory_info> memory_buffer_;

  // The previous raw samples, for the per-second deltas.
  std::map<std::string, collector_source::cpu_times> last_cpu_times_;
  std::map<std::string, collector_source::net_sample> last_net_;
  network_check::nics_type network_;
  // Whether each buffer has been tried and how the last try went (guarded by
  // mutex_, like the buffers).
  sampling::tracker cpu_sampling_;
  sampling::tracker memory_sampling_;
  sampling::tracker network_sampling_;
  // Thread-local to thread_proc(): the error each source last logged.
  std::string last_logged_cpu_error;
  std::string last_logged_memory_error;
  std::string last_logged_network_error;
  std::string last_logged_history_error;

  static std::string read_source(const std::function<void()> &read);
  static void record(sampling::tracker &tracker, bool sampled, bool baseline, const std::string &error);
  void log_sampling_error(const std::string &what, const std::string &error, std::string &last_logged);

  // Process history (keyed by lowercase exe name), tracked once per second when
  // process_history_enabled is set. Mirrors the Windows process-history feature.
  std::map<std::string, process_history_check::process_record> proc_history_;

  filters::cpu::filter_config_handler cpu_filters_;
  filters::mem::filter_config_handler mem_filters_;
  filters::proc::filter_config_handler proc_filters_;

  // See set_settings_path().
  std::string settings_path_;

 public:
  std::string subsystem;
  std::string default_buffer_size;
  // Bound directly from settings ("process history"); gates per-second /proc
  // enumeration for the history feature. Public to mirror the Windows collector.
  bool process_history_enabled;

 public:
  pdh_thread() : stop_requested_(false), core_(nullptr), plugin_id_(0), process_history_enabled(false) {}
  // Stop the collector before its buffers and mutexes go: a reload replaces
  // the instance, and dropping a joinable boost::thread detaches it.
  ~pdh_thread() { stop(); }

  void set_core(nscapi::core_wrapper *core, int plugin_id) {
    core_ = core;
    plugin_id_ = plugin_id;
  }

  bool start();
  bool stop();

  // Get CPU load averaged over the specified number of seconds
  std::map<std::string, load_entry> get_cpu_load(long seconds);

  // Check if we have collected any data yet
  bool has_cpu_data() const;

  // Get memory info averaged over the specified number of seconds
  memory_info get_memory(long seconds);

  // Check if we have collected any memory data yet
  bool has_memory_data() const;

  // Get the latest per-interface network throughput snapshot
  network_check::nics_type get_network() const;

  // Check if we have collected any network data yet
  bool has_network_data() const;

  // Whether each buffer is ready, still waiting for its first tick, or has
  // been tried and is still empty (see sampling_state.hpp).
  sampling::status cpu_status() const;
  sampling::status memory_status() const;
  sampling::status network_status() const;

  // Get the accumulated process history (empty unless tracking is enabled)
  process_history_check::history_type get_process_history() const;

  // Whether process-history tracking is enabled
  bool has_process_history() const { return process_history_enabled; }

  // The module's settings path (e.g. /settings/system/unix), used to point the
  // user at the right section in error messages. Written once at module load
  // before the collector thread starts.
  void set_settings_path(const std::string &path) { settings_path_ = path; }
  std::string get_settings_path() const { return settings_path_; }

  void set_path(const std::string &cpu_path, const std::string &mem_path, const std::string &proc_path);

  void add_realtime_cpu_filter(std::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query);
  void add_realtime_mem_filter(std::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query);
  void add_realtime_proc_filter(std::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query);

  void add_samples(std::shared_ptr<nscapi::settings_proxy> settings);

  std::string to_string() const { return "system"; }

 private:
  void thread_proc();
  void update_process_history(const std::set<std::string> &running, long long now_ts);
};

// The pure halves of the collector, exposed for unit tests: the samples come
// from collector_source, these turn two of them into what the checks report.
namespace collector_calc {
cpu_load calculate_cpu_load(const std::map<std::string, collector_source::cpu_times> &old_times,
                            const std::map<std::string, collector_source::cpu_times> &new_times);
memory_info to_memory_info(const collector_source::memory_sample &sample);
network_check::nics_type calculate_network(const std::map<std::string, collector_source::net_sample> &old_c,
                                           const std::map<std::string, collector_source::net_sample> &new_c, double dt);
}  // namespace collector_calc

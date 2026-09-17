// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/circular_buffer.hpp>
#include <boost/thread.hpp>
#include <boost/unordered_map.hpp>
#include <error/error.hpp>
#include <map>
#include <memory>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/settings/proxy.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <rrd_buffer.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <vector>

#include <set>

#include "check_network.h"
#include "check_process_history.h"
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
  // Written by stop() on the core thread, read by the collector's loop
  // condition. A plain bool gave the compiler licence to hoist the read out of
  // the loop - only the opaque sleep and log calls kept it honest - and there
  // was no happens-before between the write and the read either.
  std::atomic<bool> stop_requested_;
  // Signalled by stop() so the collector leaves its one-second wait at once.
  // sleep_for() is not interruptible, so every stop - which, since the
  // collector is rebuilt on every reload, means every reload - used to cost up
  // to a second of join latency. The Windows sibling and CheckDisk have always
  // used an interruptible primitive.
  mutable std::mutex stop_mutex_;
  std::condition_variable stop_cond_;

  // Wait up to `seconds`, returning early if a stop has been requested.
  // Returns true when the collector should keep going.
  bool wait_for_tick(int seconds) {
    std::unique_lock<std::mutex> lock(stop_mutex_);
    stop_cond_.wait_for(lock, std::chrono::seconds(seconds), [this] { return stop_requested_.load(); });
    return !stop_requested_.load();
  }

  nscapi::core_wrapper *core_;
  int plugin_id_;

  // CPU data collection
  rrd_buffer<cpu_load> cpu_buffer_;

  // Memory data collection
  rrd_buffer<memory_info> memory_buffer_;

  // Raw CPU times for delta calculation
  struct cpu_times {
    std::string name;
    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
    unsigned long long iowait;
    unsigned long long irq;
    unsigned long long softirq;
    unsigned long long steal;

    cpu_times() : user(0), nice(0), system(0), idle(0), iowait(0), irq(0), softirq(0), steal(0) {}

    unsigned long long total_idle() const { return idle + iowait; }
    unsigned long long total_busy() const { return user + nice + system + irq + softirq + steal; }
    unsigned long long total() const { return total_idle() + total_busy(); }
  };

  std::map<std::string, cpu_times> last_cpu_times_;

  // Raw cumulative /proc/net/dev counters per interface, for rate calculation.
  struct net_counters {
    unsigned long long rx_bytes = 0, rx_packets = 0, rx_errors = 0;
    unsigned long long tx_bytes = 0, tx_packets = 0, tx_errors = 0;
  };
  std::map<std::string, net_counters> last_net_;
  network_check::nics_type network_;

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
  std::map<std::string, cpu_times> read_cpu_times();
  cpu_load calculate_cpu_load(const std::map<std::string, cpu_times> &old_times, const std::map<std::string, cpu_times> &new_times);
  memory_info read_memory_info();
  std::map<std::string, net_counters> read_net_counters();
  network_check::nics_type calculate_network(const std::map<std::string, net_counters> &old_c, const std::map<std::string, net_counters> &new_c, double dt);
  std::set<std::string> read_running_exes();
  void update_process_history(const std::set<std::string> &running, long long now_ts);
};

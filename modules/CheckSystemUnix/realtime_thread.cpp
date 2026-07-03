/*
 * Copyright (C) 2004-2016 Michael Medin
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

#include "realtime_thread.hpp"

#include <dirent.h>
#include <limits.h>
#include <unistd.h>

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <chrono>
#include <fstream>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <parsers/filter/realtime_helper.hpp>
#include <sstream>
#include <str/utf8.hpp>
#include <str/xtos.hpp>
#include <thread>

#include "realtime_data.hpp"

#define STAT_FILE "/proc/stat"
#define MEMINFO_FILE "/proc/meminfo"
#define NETDEV_FILE "/proc/net/dev"

typedef parsers::where::realtime_filter_helper<checks::check_cpu_filter::runtime_data, filters::cpu::filter_config_object> cpu_filter_helper;
typedef parsers::where::realtime_filter_helper<check_memory::check_mem_filter::runtime_data, filters::mem::filter_config_object> mem_filter_helper;
typedef parsers::where::realtime_filter_helper<check_proc::check_proc_filter::runtime_data, filters::proc::filter_config_object> proc_filter_helper;

/**
 * Thread that collects the data every second and evaluates real-time filters.
 */
void pdh_thread::thread_proc() {
  // Initial read to establish baseline
  last_cpu_times_ = read_cpu_times();
  last_net_ = read_net_counters();
  auto last_net_sample = std::chrono::steady_clock::now();

  // Build real-time filter helpers from the configured objects
  cpu_filter_helper cpu_helper(core_, plugin_id_);
  mem_filter_helper mem_helper(core_, plugin_id_);
  proc_filter_helper proc_helper(core_, plugin_id_);

  for (const std::shared_ptr<filters::cpu::filter_config_object> &object : cpu_filters_.get_object_list()) {
    try {
      checks::check_cpu_filter::runtime_data data;
      if (object->data.empty()) {
        data.add("1m");
      } else {
        for (const std::string &d : object->data) {
          data.add(d);
        }
      }
      cpu_helper.add_item(object, data, "system.cpu");
    } catch (const std::exception &e) {
      NSC_LOG_ERROR_EXR("Skipping CPU filter '" + object->get_alias() + "' (invalid time spec): ", e);
    }
  }
  for (const std::shared_ptr<filters::mem::filter_config_object> &object : mem_filters_.get_object_list()) {
    check_memory::check_mem_filter::runtime_data data;
    if (object->data.empty()) {
      data.add("physical");
    } else {
      for (const std::string &d : object->data) {
        data.add(d);
      }
    }
    mem_helper.add_item(object, data, "system.memory");
  }
  for (const std::shared_ptr<filters::proc::filter_config_object> &object : proc_filters_.get_object_list()) {
    check_proc::check_proc_filter::runtime_data data;
    for (const std::string &d : object->data) {
      data.add(d);
    }
    proc_helper.add_item(object, data, "system.process");
  }

  cpu_helper.touch_all();
  mem_helper.touch_all();
  proc_helper.touch_all();

  const bool has_cpu_realtime = !cpu_filters_.empty();
  const bool has_mem_realtime = !mem_filters_.empty();
  const bool has_proc_realtime = !proc_filters_.empty();
  if (has_cpu_realtime || has_mem_realtime || has_proc_realtime) {
    NSC_DEBUG_MSG("Real time checks enabled");
  }

  while (!stop_requested_) {
    std::this_thread::sleep_for(std::chrono::seconds(1));

    if (stop_requested_) break;

    try {
      // Collect CPU data
      auto current_times = read_cpu_times();
      auto load = calculate_cpu_load(last_cpu_times_, current_times);
      last_cpu_times_ = current_times;

      // Collect memory data
      auto mem = read_memory_info();

      // Collect network data (rates over the measured sampling interval — the
      // loop target is 1s but scheduling delays and suspend/resume stretch it).
      auto net_now = read_net_counters();
      const auto net_sample_time = std::chrono::steady_clock::now();
      const double dt = std::chrono::duration<double>(net_sample_time - last_net_sample).count();
      auto nics = calculate_network(last_net_, net_now, dt);
      last_net_ = net_now;
      last_net_sample = net_sample_time;

      // Collect process history (only when explicitly enabled — it enumerates
      // /proc every second).
      std::set<std::string> running_exes;
      const bool track_history = process_history_enabled;
      if (track_history) running_exes = read_running_exes();

      {
        boost::unique_lock lock(mutex_);
        cpu_buffer_.push(load);
        memory_buffer_.push(mem);
        network_ = nics;
        if (track_history) {
          static const boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1));
          const long long now_ts = (boost::posix_time::second_clock::universal_time() - epoch).total_seconds();
          update_process_history(running_exes, now_ts);
        }
      }
    } catch (const std::exception &e) {
      NSC_LOG_ERROR("Failed to collect system data: " + std::string(e.what()));
    }

    // Evaluate real-time filters once we have collected data
    try {
      if (has_cpu_realtime) cpu_helper.process_items(this);
      if (has_mem_realtime) mem_helper.process_items(this);
      if (has_proc_realtime) proc_helper.process_items(this);
    } catch (const std::exception &e) {
      NSC_LOG_ERROR("Failed to evaluate real-time filters: " + std::string(e.what()));
    }
  }
}

std::map<std::string, pdh_thread::cpu_times> pdh_thread::read_cpu_times() {
  std::map<std::string, cpu_times> result;

  try {
    std::locale mylocale("C");
    std::ifstream file;
    file.imbue(mylocale);
    file.open(STAT_FILE);
    std::string line;

    while (std::getline(file, line)) {
      if (line.substr(0, 3) != "cpu") break;

      std::istringstream iss(line);
      cpu_times ct;
      iss >> ct.name >> ct.user >> ct.nice >> ct.system >> ct.idle >> ct.iowait >> ct.irq >> ct.softirq >> ct.steal;

      result[ct.name] = ct;
    }
  } catch (const std::exception &e) {
    NSC_LOG_ERROR("Failed to read CPU times: " + std::string(e.what()));
  }

  return result;
}

cpu_load pdh_thread::calculate_cpu_load(const std::map<std::string, cpu_times> &old_times, const std::map<std::string, cpu_times> &new_times) {
  cpu_load result;
  int core_index = 0;

  for (const auto &entry : new_times) {
    const std::string &name = entry.first;
    const cpu_times &new_ct = entry.second;

    auto old_it = old_times.find(name);
    if (old_it == old_times.end()) continue;

    const cpu_times &old_ct = old_it->second;

    unsigned long long total_diff = new_ct.total() - old_ct.total();
    if (total_diff == 0) total_diff = 1;

    double user_pct = 100.0 * (new_ct.user + new_ct.nice - old_ct.user - old_ct.nice) / total_diff;
    double kernel_pct =
        100.0 * (new_ct.system + new_ct.irq + new_ct.softirq + new_ct.steal - old_ct.system - old_ct.irq - old_ct.softirq - old_ct.steal) / total_diff;
    double idle_pct = 100.0 * (new_ct.total_idle() - old_ct.total_idle()) / total_diff;

    // Clamp values
    user_pct = std::max(0.0, std::min(100.0, user_pct));
    kernel_pct = std::max(0.0, std::min(100.0, kernel_pct));
    idle_pct = std::max(0.0, std::min(100.0, idle_pct));

    if (name == "cpu") {
      // Total CPU
      result.total = load_entry(idle_pct, user_pct, kernel_pct, -1);
    } else {
      // Individual core (cpuN)
      load_entry core_entry(idle_pct, user_pct, kernel_pct, core_index++);
      result.core.push_back(core_entry);
    }
  }

  result.cores = static_cast<int>(result.core.size());
  return result;
}

bool pdh_thread::has_cpu_data() const {
  boost::shared_lock lock(mutex_);
  return cpu_buffer_.has_data();
}

std::map<std::string, load_entry> pdh_thread::get_cpu_load(long seconds) {
  std::map<std::string, load_entry> ret;

  const boost::shared_lock lock(mutex_);

  try {
    cpu_load load = cpu_buffer_.get_average(seconds);
    ret["total"] = load.total;
    int i = 0;
    for (const load_entry &l : load.core) {
      ret["core " + str::xtos(i++)] = l;
    }
  } catch (const std::exception &e) {
    NSC_LOG_ERROR("Failed to get CPU average: " + std::string(e.what()));
  }

  return ret;
}

long long read_mem_line(std::istringstream &iss) {
  std::string unit;
  unsigned long long value;
  iss >> value >> unit;
  if (unit == "kB") {
    value *= 1024;
  }
  return value;
}

memory_info pdh_thread::read_memory_info() {
  memory_info result;

  try {
    long long cached = 0;
    long long mem_free = 0;
    long long mem_total = 0;
    std::locale mylocale("C");
    std::ifstream file;
    file.imbue(mylocale);
    file.open(MEMINFO_FILE);
    std::string line;

    while (std::getline(file, line)) {
      std::istringstream iss(line);
      std::string tag;
      iss >> tag;
      if (tag == "MemTotal:")
        mem_total = read_mem_line(iss);
      else if (tag == "MemFree:")
        mem_free = read_mem_line(iss);
      else if (tag == "Buffers:" || tag == "Cached:")
        cached += read_mem_line(iss);
      else if (tag == "SwapTotal:")
        result.swap.total = read_mem_line(iss);
      else if (tag == "SwapFree:")
        result.swap.free = read_mem_line(iss);
    }

    result.physical.total = mem_total;
    result.physical.free = mem_free;
    // Cached memory: total is physical total, free is physical free + buffers/cached
    result.cached.total = mem_total;
    result.cached.free = mem_free + cached;

  } catch (const std::exception &e) {
    NSC_LOG_ERROR("Failed to read memory info: " + std::string(e.what()));
  }

  return result;
}

bool pdh_thread::has_memory_data() const {
  boost::shared_lock lock(mutex_);
  return memory_buffer_.has_data();
}

memory_info pdh_thread::get_memory(long seconds) {
  memory_info ret;

  const boost::shared_lock lock(mutex_);

  try {
    ret = memory_buffer_.get_average(seconds);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR("Failed to get memory average: " + std::string(e.what()));
  }

  return ret;
}

namespace {
// Read a single-line value from a /sys file, trimming whitespace. Returns ""
// when the file is missing/unreadable.
std::string read_sys_string(const std::string &path) {
  try {
    std::ifstream f(path.c_str());
    if (!f.is_open()) return "";
    std::string line;
    std::getline(f, line);
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' ')) line.pop_back();
    return line;
  } catch (...) {
    return "";
  }
}
}  // namespace

std::map<std::string, pdh_thread::net_counters> pdh_thread::read_net_counters() {
  std::map<std::string, net_counters> result;
  try {
    std::locale mylocale("C");
    std::ifstream file;
    file.imbue(mylocale);
    file.open(NETDEV_FILE);
    std::string line;
    int header = 0;
    while (std::getline(file, line)) {
      // Skip the two header lines.
      if (header < 2) {
        ++header;
        continue;
      }
      const std::size_t colon = line.find(':');
      if (colon == std::string::npos) continue;
      std::string name = line.substr(0, colon);
      boost::trim(name);
      std::istringstream iss(line.substr(colon + 1));
      // Receive: bytes packets errs drop fifo frame compressed multicast
      // Transmit: bytes packets errs drop fifo colls carrier compressed
      unsigned long long v[16] = {0};
      int n = 0;
      while (n < 16 && (iss >> v[n])) ++n;
      net_counters c;
      c.rx_bytes = v[0];
      c.rx_packets = v[1];
      c.rx_errors = v[2];
      c.tx_bytes = v[8];
      c.tx_packets = v[9];
      c.tx_errors = v[10];
      result[name] = c;
    }
  } catch (const std::exception &e) {
    NSC_LOG_ERROR("Failed to read network counters: " + std::string(e.what()));
  }
  return result;
}

network_check::nics_type pdh_thread::calculate_network(const std::map<std::string, net_counters> &old_c, const std::map<std::string, net_counters> &new_c,
                                                       double dt) {
  network_check::nics_type result;
  if (dt <= 0) dt = 1.0;
  auto delta = [](unsigned long long cur, unsigned long long old) { return cur >= old ? cur - old : 0ull; };

  for (const auto &entry : new_c) {
    const std::string &name = entry.first;
    const net_counters &nc = entry.second;

    network_check::network_interface nif;
    nif.name = name;
    nif.rx_errors = static_cast<long long>(nc.rx_errors);
    nif.tx_errors = static_cast<long long>(nc.tx_errors);

    auto it = old_c.find(name);
    if (it != old_c.end()) {
      const net_counters &oc = it->second;
      nif.rx_bytes_per_sec = static_cast<long long>(delta(nc.rx_bytes, oc.rx_bytes) / dt);
      nif.tx_bytes_per_sec = static_cast<long long>(delta(nc.tx_bytes, oc.tx_bytes) / dt);
      nif.rx_packets_per_sec = static_cast<long long>(delta(nc.rx_packets, oc.rx_packets) / dt);
      nif.tx_packets_per_sec = static_cast<long long>(delta(nc.tx_packets, oc.tx_packets) / dt);
    }

    // Metadata from /sys/class/net/<name>/ (best effort).
    const std::string base = "/sys/class/net/" + name + "/";
    nif.status = read_sys_string(base + "operstate");
    if (nif.status.empty()) nif.status = "unknown";
    nif.mac = read_sys_string(base + "address");
    const std::string speed = read_sys_string(base + "speed");
    if (!speed.empty()) {
      try {
        const long long mbit = std::stoll(speed);
        if (mbit > 0) nif.speed_bps = mbit * 1000000ll;
      } catch (...) {
      }
    }

    result.push_back(nif);
  }
  return result;
}

network_check::nics_type pdh_thread::get_network() const {
  const boost::shared_lock lock(mutex_);
  return network_;
}

bool pdh_thread::has_network_data() const {
  boost::shared_lock lock(mutex_);
  return !network_.empty();
}

std::set<std::string> pdh_thread::read_running_exes() {
  std::set<std::string> result;
  DIR *proc_dir = opendir("/proc");
  if (!proc_dir) return result;
  struct dirent *entry;
  while ((entry = readdir(proc_dir)) != nullptr) {
    const std::string name = entry->d_name;
    if (name.empty() || !std::all_of(name.begin(), name.end(), ::isdigit)) continue;
    // Prefer /proc/<pid>/exe: /proc/<pid>/comm is truncated to 15 chars by the
    // kernel and would never match the full executable names check_process
    // reports. readlink(exe) needs ptrace-level access, so an unprivileged
    // agent gets EACCES for other users' processes; fall back to comm,
    // extended via the world-readable cmdline when comm looks truncated.
    std::string exe;
    char exe_path[PATH_MAX] = {0};
    const std::string exe_link = "/proc/" + name + "/exe";
    const ssize_t len = readlink(exe_link.c_str(), exe_path, sizeof(exe_path) - 1);
    if (len > 0) {
      std::string full(exe_path, static_cast<std::size_t>(len));
      // The kernel appends " (deleted)" when the on-disk binary was replaced
      // (e.g. package upgrade); the process itself is still the same exe.
      const std::string deleted = " (deleted)";
      if (full.size() > deleted.size() && full.compare(full.size() - deleted.size(), deleted.size(), deleted) == 0) full.resize(full.size() - deleted.size());
      const std::size_t pos = full.find_last_of('/');
      exe = pos == std::string::npos ? full : full.substr(pos + 1);
    } else {
      try {
        std::ifstream comm("/proc/" + name + "/comm");
        if (comm.is_open()) {
          std::getline(comm, exe);
          boost::trim(exe);
        }
      } catch (...) {
      }
      // A maximal-length comm (15 chars) is likely a truncated longer name.
      // argv[0]'s basename is readable regardless of privileges; trust it only
      // when it extends the comm prefix, so processes that rewrite their argv
      // (nginx, postgres) cannot corrupt the recorded name.
      if (exe.size() == 15) {
        try {
          std::ifstream cmdline("/proc/" + name + "/cmdline");
          std::string argv0;
          if (cmdline.is_open() && std::getline(cmdline, argv0, '\0') && !argv0.empty()) {
            const std::size_t pos = argv0.find_last_of('/');
            const std::string base = pos == std::string::npos ? argv0 : argv0.substr(pos + 1);
            if (base.size() > exe.size() && base.compare(0, exe.size(), exe) == 0) exe = base;
          }
        } catch (...) {
        }
      }
    }
    if (!exe.empty()) result.insert(exe);
  }
  closedir(proc_dir);
  return result;
}

// Caller must hold the unique (write) lock on mutex_.
void pdh_thread::update_process_history(const std::set<std::string> &running, long long now_ts) {
  std::set<std::string> running_keys;
  for (const std::string &exe : running) {
    const std::string key = boost::algorithm::to_lower_copy(exe);
    running_keys.insert(key);
    auto it = proc_history_.find(key);
    if (it == proc_history_.end()) {
      proc_history_[key] = process_history_check::process_record(exe, now_ts);
    } else {
      it->second.last_seen = now_ts;
      // times_seen counts starts (as on Windows): only bump on a
      // not-running -> running transition, not on every sample.
      if (!it->second.currently_running) it->second.times_seen += 1;
      it->second.currently_running = true;
    }
  }
  for (auto &kv : proc_history_) {
    if (running_keys.count(kv.first) == 0) kv.second.currently_running = false;
  }
}

process_history_check::history_type pdh_thread::get_process_history() const {
  process_history_check::history_type result;
  const boost::shared_lock lock(mutex_);
  for (const auto &kv : proc_history_) {
    result.push_back(kv.second);
  }
  return result;
}

bool pdh_thread::start() {
  stop_requested_ = false;
  thread_ = std::shared_ptr<boost::thread>(new boost::thread([this]() { this->thread_proc(); }));
  return true;
}

bool pdh_thread::stop() {
  stop_requested_ = true;
  if (thread_) {
    thread_->join();
  }
  return true;
}

void pdh_thread::add_realtime_cpu_filter(std::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query) {
  try {
    cpu_filters_.add(proxy, key, query);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("Failed to add cpu real-time filter: " + key, e);
  } catch (...) {
    NSC_LOG_ERROR_EX("Failed to add cpu real-time filter: " + key);
  }
}

void pdh_thread::add_realtime_mem_filter(std::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query) {
  try {
    mem_filters_.add(proxy, key, query);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("Failed to add memory real-time filter: " + key, e);
  } catch (...) {
    NSC_LOG_ERROR_EX("Failed to add memory real-time filter: " + key);
  }
}

void pdh_thread::add_realtime_proc_filter(std::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query) {
  try {
    proc_filters_.add(proxy, key, query);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("Failed to add process real-time filter: " + key, e);
  } catch (...) {
    NSC_LOG_ERROR_EX("Failed to add process real-time filter: " + key);
  }
}

void pdh_thread::set_path(const std::string &cpu_path, const std::string &mem_path, const std::string &proc_path) {
  cpu_filters_.set_path(cpu_path);
  mem_filters_.set_path(mem_path);
  proc_filters_.set_path(proc_path);
}

void pdh_thread::add_samples(std::shared_ptr<nscapi::settings_proxy> settings) {
  cpu_filters_.add_samples(settings);
  mem_filters_.add_samples(settings);
  proc_filters_.add_samples(settings);
}

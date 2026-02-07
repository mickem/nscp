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

#include <chrono>
#include <fstream>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <parsers/filter/realtime_helper.hpp>
#include <sstream>
#include <str/xtos.hpp>
#include <thread>

#include "realtime_data.hpp"

#define STAT_FILE "/proc/stat"
#define MEMINFO_FILE "/proc/meminfo"

/**
 * Thread that collects the data every second.
 */
void pdh_thread::thread_proc() {
  // Initial read to establish baseline
  last_cpu_times_ = read_cpu_times();

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

      {
        boost::unique_lock lock(mutex_);
        cpu_buffer_.push(load);
        memory_buffer_.push(mem);
      }
    } catch (const std::exception &e) {
      NSC_LOG_ERROR("Failed to collect system data: " + std::string(e.what()));
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

  const boost::shared_lock lock(mutex_, boost::try_to_lock);
  if (!lock.owns_lock()) {
    NSC_LOG_ERROR("Failed to get mutex for CPU data");
    return ret;
  }

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

  const boost::shared_lock lock(mutex_, boost::try_to_lock);
  if (!lock.owns_lock()) {
    NSC_LOG_ERROR("Failed to get mutex for memory data");
    return ret;
  }

  try {
    ret = memory_buffer_.get_average(seconds);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR("Failed to get memory average: " + std::string(e.what()));
  }

  return ret;
}

bool pdh_thread::start() {
  stop_requested_ = false;
  thread_ = boost::shared_ptr<boost::thread>(new boost::thread([this]() { this->thread_proc(); }));
  return true;
}

bool pdh_thread::stop() {
  stop_requested_ = true;
  if (thread_) {
    thread_->join();
  }
  return true;
}

void pdh_thread::add_realtime_filter(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query) {
  // Not implemented for Unix
}

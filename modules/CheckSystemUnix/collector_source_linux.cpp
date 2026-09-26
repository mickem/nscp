// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The collector's samples on Linux: /proc/stat, /proc/meminfo, /proc/net/dev
// with the link metadata from /sys/class/net, and the /proc/<pid> tree for
// process history.

#include <dirent.h>
#include <limits.h>
#include <unistd.h>

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <fstream>
#include <locale>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <sstream>

#include "collector_source.h"

namespace collector_source {

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

unsigned long long read_mem_line(std::istringstream &iss) {
  std::string unit;
  unsigned long long value = 0;
  iss >> value >> unit;
  if (unit == "kB") {
    value *= 1024;
  }
  return value;
}

}  // namespace

std::map<std::string, cpu_times> read_cpu_times() {
  std::map<std::string, cpu_times> result;

  try {
    std::locale mylocale("C");
    std::ifstream file;
    file.imbue(mylocale);
    file.open("/proc/stat");
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

memory_sample read_memory() {
  memory_sample result;

  try {
    unsigned long long cached = 0;
    std::locale mylocale("C");
    std::ifstream file;
    file.imbue(mylocale);
    file.open("/proc/meminfo");
    std::string line;

    while (std::getline(file, line)) {
      std::istringstream iss(line);
      std::string tag;
      iss >> tag;
      if (tag == "MemTotal:") {
        result.physical_total = read_mem_line(iss);
      } else if (tag == "MemFree:") {
        result.physical_free = read_mem_line(iss);
      } else if (tag == "Buffers:" || tag == "Cached:") {
        cached += read_mem_line(iss);
      } else if (tag == "SwapTotal:") {
        result.swap_total = read_mem_line(iss);
      } else if (tag == "SwapFree:") {
        result.swap_free = read_mem_line(iss);
      }
    }

    // Cached memory: total is physical total, free is physical free + buffers/cached
    result.cached_free = result.physical_free + cached;
  } catch (const std::exception &e) {
    NSC_LOG_ERROR("Failed to read memory info: " + std::string(e.what()));
  }

  return result;
}

std::map<std::string, unsigned long long> read_memory_extras() { return {}; }

std::map<std::string, net_sample> read_network() {
  std::map<std::string, net_sample> result;
  try {
    std::locale mylocale("C");
    std::ifstream file;
    file.imbue(mylocale);
    file.open("/proc/net/dev");
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
      net_sample c;
      c.rx_bytes = v[0];
      c.rx_packets = v[1];
      c.rx_errors = v[2];
      c.tx_bytes = v[8];
      c.tx_packets = v[9];
      c.tx_errors = v[10];

      // Metadata from /sys/class/net/<name>/ (best effort).
      const std::string base = "/sys/class/net/" + name + "/";
      c.status = read_sys_string(base + "operstate");
      c.mac = read_sys_string(base + "address");
      const std::string speed = read_sys_string(base + "speed");
      if (!speed.empty()) {
        try {
          const long long mbit = std::stoll(speed);
          if (mbit > 0) c.speed_bps = mbit * 1000000ll;
        } catch (...) {
        }
      }
      result[name] = c;
    }
  } catch (const std::exception &e) {
    NSC_LOG_ERROR("Failed to read network counters: " + std::string(e.what()));
  }
  return result;
}

std::set<std::string> read_running_exes() {
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

}  // namespace collector_source

// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// check_process's reader on Linux: every numeric directory under /proc, read
// through the pure /proc parsers in check_process.cpp.

#include <dirent.h>
#include <limits.h>
#include <unistd.h>

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "check_process.h"

namespace check_proc {
namespace check_proc_filter {

namespace {

std::string read_file(const std::string &path) {
  std::ifstream file(path);
  std::stringstream ss;
  ss << file.rdbuf();
  return ss.str();
}

// Boot time is constant for the lifetime of the agent; read it once.
unsigned long long get_boot_time() {
  static const unsigned long long boot_time = [] {
    unsigned long long btime = 0;
    parse_proc_stat_btime(read_file("/proc/stat"), btime);
    return btime;
  }();
  return boot_time;
}

}  // namespace

// Read process information from /proc
filter_obj read_process_info(int pid, bool resolve_owner = false) {
  filter_obj info;
  info.pid = pid;
  info.started = true;

  std::string proc_path = "/proc/" + std::to_string(pid);

  // Read /proc/[pid]/exe (symlink to executable)
  try {
    char exe_path[PATH_MAX];
    std::string exe_link = proc_path + "/exe";
    ssize_t len = readlink(exe_link.c_str(), exe_path, sizeof(exe_path) - 1);
    if (len != -1) {
      exe_path[len] = '\0';
      info.filename = std::string(exe_path);
      // Extract just the executable name
      std::size_t pos = info.filename.find_last_of('/');
      if (pos != std::string::npos)
        info.exe = info.filename.substr(pos + 1);
      else
        info.exe = info.filename;
    }
  } catch (...) {
    info.error = "Cannot read exe link";
  }

  try {
    std::ifstream cmdline_file(proc_path + "/cmdline");
    if (cmdline_file.is_open()) {
      std::string cmdline;
      std::getline(cmdline_file, cmdline, '\0');
      // cmdline uses null bytes as separators, replace with spaces
      std::string full_cmdline;
      while (cmdline_file.good()) {
        if (!full_cmdline.empty()) full_cmdline += " ";
        full_cmdline += cmdline;
        std::getline(cmdline_file, cmdline, '\0');
      }
      if (!full_cmdline.empty()) {
        info.command_line = full_cmdline;
      } else if (!cmdline.empty()) {
        info.command_line = cmdline;
      }
    }
  } catch (...) {
    info.error = "Cannot read cmdline";
  }

  // If we couldn't get exe from /exe symlink, try to get it from cmdline or comm
  if (info.exe.empty()) {
    // Try /proc/[pid]/comm
    try {
      std::ifstream comm_file(proc_path + "/comm");
      if (comm_file.is_open()) {
        std::getline(comm_file, info.exe);
        boost::trim(info.exe);
      }
    } catch (...) {
    }

    // If still empty, try from command line
    if (info.exe.empty() && !info.command_line.empty()) {
      std::size_t pos = info.command_line.find(' ');
      std::string first_arg = (pos != std::string::npos) ? info.command_line.substr(0, pos) : info.command_line;
      pos = first_arg.find_last_of('/');
      info.exe = (pos != std::string::npos) ? first_arg.substr(pos + 1) : first_arg;
    }
  }

  // Read /proc/[pid]/stat for status and other info
  try {
    std::ifstream stat_file(proc_path + "/stat");
    if (stat_file.is_open()) {
      std::string line;
      std::getline(stat_file, line);

      proc_stat_data stat_data;
      if (parse_proc_pid_stat(line, stat_data)) {
        // State: R=running, S=sleeping, D=disk sleep, Z=zombie, T=stopped, t=tracing stop, X=dead
        info.started = (stat_data.state == 'R' || stat_data.state == 'S' || stat_data.state == 'D');
        info.proc_state = stat_data.state;
        info.ppid = stat_data.ppid;

        info.user_time_raw = stat_data.utime_jiffies;
        info.kernel_time_raw = stat_data.stime_jiffies;
        info.start_time_jiffies = stat_data.starttime_jiffies;

        // Convert jiffies to seconds (typically 100 Hz = USER_HZ)
        long ticks_per_sec = sysconf(_SC_CLK_TCK);
        const unsigned long long boot_time = get_boot_time();
        if (ticks_per_sec > 0) {
          info.user_time = stat_data.utime_jiffies / ticks_per_sec;
          info.kernel_time = stat_data.stime_jiffies / ticks_per_sec;
          info.creation_time = boot_time + stat_data.starttime_jiffies / ticks_per_sec;
          // Only meaningful once the boot time is known; without it
          // creation_time is an offset from the epoch and the elapsed seconds
          // would be nonsense rather than merely imprecise.
          if (boot_time != 0) {
            const unsigned long long now = static_cast<unsigned long long>(::time(nullptr));
            info.elapsed = now > info.creation_time ? now - info.creation_time : 0;
          }
        }
        info.total_time = info.user_time + info.kernel_time;

        info.page_faults = stat_data.major_faults;
      }
    }
  } catch (...) {
    info.error = "Cannot read stat";
  }

  // Read /proc/[pid]/status for the peak memory counters and the owner uid.
  // Kernel threads have no Vm* entries; the peaks then stay 0.
  try {
    std::ifstream status_file(proc_path + "/status");
    if (status_file.is_open()) {
      std::stringstream ss;
      ss << status_file.rdbuf();
      const std::string content = ss.str();
      parse_proc_status_bytes(content, "VmPeak", info.peak_virtual_size);
      parse_proc_status_bytes(content, "VmHWM", info.peak_working_set);
      // The uid itself is free (it is in the file we just read); turning it
      // into a name is what costs, so that stays behind resolve-owner.
      parse_proc_status_uid(content, info.uid);
      if (resolve_owner) info.username = lookup_username(info.uid);
    }
  } catch (...) {
    info.error = "Cannot read status";
  }

  // Read /proc/[pid]/statm for memory info
  try {
    std::ifstream statm_file(proc_path + "/statm");
    if (statm_file.is_open()) {
      unsigned long size, resident, shared, text, lib, data, dt;
      statm_file >> size >> resident >> shared >> text >> lib >> data >> dt;

      long page_size = sysconf(_SC_PAGESIZE);
      if (page_size > 0) {
        info.virtual_size = size * page_size;
        info.working_set = resident * page_size;
      }
    }
  } catch (...) {
    info.error = "Cannot read statm";
  }

  return info;
}

// Enumerate all processes from /proc
std::vector<filter_obj> enumerate_processes(bool resolve_owner) {
  std::vector<filter_obj> result;

  DIR *proc_dir = opendir("/proc");
  if (!proc_dir) {
    return result;
  }

  struct dirent *entry;
  while ((entry = readdir(proc_dir)) != nullptr) {
    // Check if the entry is a PID directory (all digits)
    std::string name = entry->d_name;
    bool is_pid = !name.empty() && std::all_of(name.begin(), name.end(), ::isdigit);

    if (is_pid) {
      int pid = std::stoi(name);
      try {
        filter_obj info = read_process_info(pid, resolve_owner);
        if (!info.exe.empty() || !info.command_line.empty()) {
          result.push_back(info);
        }
      } catch (...) {
        // Skip processes we can't read
      }
    }
  }

  closedir(proc_dir);
  return result;
}

bool read_cpu_capacity(unsigned long long &capacity) { return parse_proc_stat_cpu_total(read_file("/proc/stat"), capacity); }

}  // namespace check_proc_filter
}  // namespace check_proc

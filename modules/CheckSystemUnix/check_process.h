// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#ifndef NSCP_CHECK_PROCESS_H
#define NSCP_CHECK_PROCESS_H
#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <str/format.hpp>
#include <string>
#include <vector>

namespace check_proc {

namespace check_proc_filter {

struct filter_obj {
  std::string filename;
  std::string exe;
  std::string command_line;
  int pid = 0;
  bool started = false;
  std::string error;

  // Memory counters
  unsigned long long virtual_size = 0;
  unsigned long long peak_virtual_size = 0;
  unsigned long long working_set = 0;
  unsigned long long peak_working_set = 0;
  unsigned long long page_faults = 0;

  // Time counters. Normally cumulative CPU seconds; with delta=true
  // make_cpu_delta() re-populates them as whole percentages of total CPU
  // capacity consumed over the one second sample window.
  unsigned long long user_time = 0;
  unsigned long long kernel_time = 0;
  unsigned long long total_time = 0;

  // Raw cumulative CPU jiffies from /proc/[pid]/stat, kept absolute so
  // make_cpu_delta() can compute the delta between two snapshots.
  unsigned long long user_time_raw = 0;
  unsigned long long kernel_time_raw = 0;

  // Process start: starttime from /proc/[pid]/stat (jiffies since boot, which
  // uniquely identifies the PID incarnation) and the derived absolute unix
  // timestamp exposed as the `creation` keyword.
  unsigned long long start_time_jiffies = 0;
  unsigned long long creation_time = 0;

  filter_obj() {}

  filter_obj(const std::string &exe_name) : exe(exe_name) {}

  filter_obj(const filter_obj &other) = default;

  std::string show() const { return filename + ", " + command_line + ", pid: " + std::to_string(pid) + ", state: " + get_state_s(); }

  std::string get_filename() const { return filename; }
  std::string get_exe() const { return exe; }
  std::string get_command_line() const { return command_line; }
  std::string get_error() const { return error; }
  long long get_pid() const { return pid; }

  bool get_started() const { return started; }
  bool get_stopped() const { return !started; }
  bool get_has_error() const { return !error.empty(); }

  std::string get_state_s() const {
    if (!error.empty()) return "error";
    if (started) return "started";
    return "stopped";
  }

  static constexpr long long state_started = 1;
  static constexpr long long state_stopped = 0;
  static constexpr long long state_unknown = -10;

  long long get_state_i() const {
    if (started) return state_started;
    return state_stopped;
  }

  static long long parse_state(const std::string &s) {
    if (s == "started") return state_started;
    if (s == "stopped") return state_stopped;
    return state_unknown;
  }

  // Memory getters
  long long get_virtual_size() const { return virtual_size; }
  long long get_peak_virtual_size() const { return peak_virtual_size; }
  long long get_working_set() const { return working_set; }
  long long get_peak_working_set() const { return peak_working_set; }
  long long get_page_faults() const { return page_faults; }

  std::string get_virtual_size_human() const { return str::format::format_byte_units(virtual_size); }
  std::string get_peak_virtual_size_human() const { return str::format::format_byte_units(peak_virtual_size); }
  std::string get_working_set_human() const { return str::format::format_byte_units(working_set); }
  std::string get_peak_working_set_human() const { return str::format::format_byte_units(peak_working_set); }

  // Time getters
  long long get_user_time() const { return user_time; }
  long long get_kernel_time() const { return kernel_time; }
  long long get_total_time() const { return total_time; }
  long long get_creation_time() const { return creation_time; }

  // Compute `part * 100 / whole` rounded to the nearest whole percent rather
  // than truncated, so small-but-real usage stays visible (same rounding as
  // the Windows check after the delta=true reliability fix).
  static unsigned long long to_percent(const unsigned long long part, const unsigned long long whole) {
    if (whole == 0) return 0;
    return (part * 100ull + whole / 2ull) / whole;
  }

  // Turn this snapshot into per-process CPU usage over the sampling interval,
  // using `previous` as the earlier snapshot of the SAME process.
  // `capacity_jiffies` is the total system CPU time consumed over the same
  // interval (every field of the aggregate cpu line of /proc/stat, i.e. all
  // cores including idle), so a single-threaded busy process reads
  // 100/ncores % — the same all-cores normalization Windows gets from
  // GetSystemTimes().
  //
  // On success kernel_time, user_time and total_time hold whole-percent CPU
  // usage and the function returns true. It returns false - and zeroes the
  // fields - when the delta is not meaningful and the caller should drop the
  // process: when a raw counter moved backwards (the PID was recycled to a
  // different process mid-interval) or when no capacity was measured.
  bool make_cpu_delta(const filter_obj &previous, const unsigned long long capacity_jiffies) {
    if (capacity_jiffies == 0 || kernel_time_raw < previous.kernel_time_raw || user_time_raw < previous.user_time_raw) {
      kernel_time = 0;
      user_time = 0;
      total_time = 0;
      return false;
    }
    const unsigned long long kernel_delta = kernel_time_raw - previous.kernel_time_raw;
    const unsigned long long user_delta = user_time_raw - previous.user_time_raw;
    kernel_time = to_percent(kernel_delta, capacity_jiffies);
    user_time = to_percent(user_delta, capacity_jiffies);
    total_time = to_percent(kernel_delta + user_delta, capacity_jiffies);
    return true;
  }

  // For aggregation
  filter_obj &operator+=(const filter_obj &other) {
    // State: the total row reports "started" if any aggregated process is.
    // Without this the total is always "stopped", which trips the default
    // critical filter `state = 'stopped'`.
    started = started || other.started;
    virtual_size += other.virtual_size;
    peak_virtual_size += other.peak_virtual_size;
    working_set += other.working_set;
    peak_working_set += other.peak_working_set;
    page_faults += other.page_faults;
    user_time += other.user_time;
    kernel_time += other.kernel_time;
    total_time += other.total_time;
    return *this;
  }
};

// Parsed subset of /proc/[pid]/stat.
struct proc_stat_data {
  std::string comm;
  char state = '?';
  unsigned long long major_faults = 0;
  unsigned long long utime_jiffies = 0;
  unsigned long long stime_jiffies = 0;
  unsigned long long starttime_jiffies = 0;
};

// Parse one line of /proc/[pid]/stat. The comm field is enclosed in
// parentheses and may itself contain spaces and ") (" sequences; everything
// between the first '(' and the LAST ')' is the process name. Returns false
// if the line is malformed.
bool parse_proc_pid_stat(const std::string &line, proc_stat_data &data);

// Find `key` (e.g. "VmHWM") in /proc/[pid]/status content and return its
// value converted from kB to bytes. Returns false when the key is missing
// (kernel threads have no Vm* entries).
bool parse_proc_status_bytes(const std::string &content, const std::string &key, unsigned long long &bytes);

// Total CPU jiffies (all cores, including idle) from the aggregate "cpu "
// line of /proc/stat. guest/guest_nice are already folded into user/nice by
// the kernel and are NOT summed again.
bool parse_proc_stat_cpu_total(const std::string &content, unsigned long long &total_jiffies);

// Boot time (unix timestamp) from the btime line of /proc/stat.
bool parse_proc_stat_btime(const std::string &content, unsigned long long &btime);

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

// Enumerate every process from /proc (used by check_process and the real-time
// process filter).
std::vector<filter_obj> enumerate_processes();

// Enumerate processes twice, one second apart, and report user/kernel/time as
// whole-percent CPU usage over that window (delta=true mode).
std::vector<filter_obj> enumerate_processes_delta();

}  // namespace check_proc_filter

void check_process(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace check_proc

#endif  // NSCP_CHECK_PROCESS_H

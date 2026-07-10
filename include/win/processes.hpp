// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <list>
#include <memory>
#include <str/format.hpp>
#include <str/xtos.hpp>
#include <string>
#include <utility>
#include <win/windows.hpp>

#define DEFAULT_BUFFER_SIZE 4096

namespace win_list_processes {
template <class T>
struct int_var {
  T value;
  int_var() : value(0) {}
  T get() const { return value; }
  int_var &operator=(const T v) {
    value = v;
    return *this;
  }
  // ReSharper disable once CppNonExplicitConversionOperator
  operator T() const { return value; }
  int_var &operator+=(T other) {
    value += other;
    return *this;
  }
  int_var &operator-=(T other) {
    value -= other;
    return *this;
  }
  void delta(T previous, T total) {
    if (total == 0) {
      value = 0;
    } else {
      value = 100 * (value - previous) / total;
    }
  }
};
typedef int_var<long long> ll_var;
typedef int_var<unsigned long long> ull_var;

struct bool_var {
  bool value;
  bool_var() : value(false) {}
  bool get() const { return value; }
  bool_var &operator=(const bool v) {
    value = v;
    return *this;
  }
  // ReSharper disable once CppNonExplicitConversionOperator
  operator bool() const { return value; }
};
struct string_var {
  std::string value;
  string_var() = default;
  explicit string_var(std::string s) : value(std::move(s)) {}
  std::string get() const { return value; }
  string_var &operator=(const std::string &v) {
    value = v;
    return *this;
  }
  // ReSharper disable once CppNonExplicitConversionOperator
  operator std::string() const { return value; }
};

#define STR_GETTER(name) \
  std::string get_##name() const { return (name).get(); }
#define INT_GETTER(name) \
  long long get_##name() const { return (name).get(); }
#define BOL_GETTER(name) \
  bool get_##name() const { return (name).get(); }
#define HUMAN_SIZE_GETTER(name) \
  std::string get_##name##_human() const { return str::format::format_byte_units((name).get()); }

struct process_info {
  string_var filename;
  string_var command_line;
  string_var exe;
  STR_GETTER(filename);
  STR_GETTER(command_line);
  STR_GETTER(exe);

  int_var<DWORD> pid;
  INT_GETTER(pid);

  bool started{};
  bool_var is_new;
  bool_var hung;
  bool_var wow64;
  bool_var has_error;
  bool_var unreadable;
  string_var error;
  bool get_started() const { return started; }
  BOL_GETTER(hung);
  BOL_GETTER(wow64);
  BOL_GETTER(has_error);
  BOL_GETTER(unreadable);
  STR_GETTER(error);

  process_info() = default;
  explicit process_info(const std::string &s) : exe(s) {}

  std::string show() const { return filename.get() + ", " + command_line.get() + ", pid: " + str::xtos(pid.get()) + ", state: " + get_state_s(); }

  std::string get_state_s() const {
    if (has_error) return "error";
    if (unreadable) return "unreadable";
    if (hung) return "hung";
    if (started) return "started";
    return "stopped";
  }
  std::string get_legacy_state_s() const {
    if (unreadable) return "unreadable";
    if (hung) return "hung";
    if (started) return "Running";
    return "not running";
  }
  bool get_stopped() const { return !started; }
  bool get_is_new() const { return is_new.get(); }

  // Handles
  ll_var handleCount;
  ll_var gdiHandleCount;
  ll_var userHandleCount;
  INT_GETTER(handleCount);
  INT_GETTER(gdiHandleCount);
  INT_GETTER(userHandleCount);

  // Thread count. Populated from a single system-wide thread snapshot during enumeration.
  ll_var threadCount;
  INT_GETTER(threadCount);

  // Times
  ull_var creation_time;
  ull_var kernel_time;
  unsigned long long user_time_raw{};
  unsigned long long kernel_time_raw{};
  ull_var user_time;
  ull_var total_time;
  INT_GETTER(creation_time);
  INT_GETTER(kernel_time);
  INT_GETTER(user_time);
  INT_GETTER(total_time);

  // IO Counters
  ull_var readOperationCount;
  ull_var writeOperationCount;
  ull_var otherOperationCount;
  ull_var readTransferCount;
  ull_var writeTransferCount;
  ull_var otherTransferCount;

  // Mem Counters
  ull_var PeakVirtualSize;
  ull_var VirtualSize;
  ull_var PageFaultCount;
  ull_var PeakWorkingSetSize;
  ull_var WorkingSetSize;
  ull_var QuotaPeakPagedPoolUsage;
  ull_var QuotaPagedPoolUsage;
  ull_var QuotaPeakNonPagedPoolUsage;
  ull_var QuotaNonPagedPoolUsage;
  ull_var PageFileUsage;
  ull_var PeakPageFileUsage;
  INT_GETTER(PeakVirtualSize);
  HUMAN_SIZE_GETTER(PeakVirtualSize);
  INT_GETTER(VirtualSize);
  HUMAN_SIZE_GETTER(VirtualSize);
  INT_GETTER(PageFaultCount);
  INT_GETTER(PeakWorkingSetSize);
  HUMAN_SIZE_GETTER(PeakWorkingSetSize);
  INT_GETTER(WorkingSetSize);
  HUMAN_SIZE_GETTER(WorkingSetSize);
  INT_GETTER(QuotaPeakPagedPoolUsage);
  INT_GETTER(QuotaPagedPoolUsage);
  INT_GETTER(QuotaPeakNonPagedPoolUsage);
  INT_GETTER(QuotaNonPagedPoolUsage);
  INT_GETTER(PageFileUsage);
  HUMAN_SIZE_GETTER(PageFileUsage);
  INT_GETTER(PeakPageFileUsage);
  HUMAN_SIZE_GETTER(PeakPageFileUsage);

  // System-wide totals captured once during enumeration, used as the base for
  // the percentage getters below so users get MemoryWarning '10%'
  // ergonomics instead of only absolute bytes. total_physical_memory is
  // GlobalMemoryStatusEx ullTotalPhys; total_pagefile is ullTotalPageFile
  // (the commit limit = RAM + pagefile).
  unsigned long long total_physical_memory{};
  unsigned long long total_pagefile{};
  // Working set as a percentage of total physical RAM.
  long long get_working_set_pct() const {
    return total_physical_memory ? static_cast<long long>(to_percent(WorkingSetSize.get(), total_physical_memory)) : 0;
  }
  // Pagefile (commit) usage as a percentage of the system commit limit.
  long long get_pagefile_pct() const { return total_pagefile ? static_cast<long long>(to_percent(PageFileUsage.get(), total_pagefile)) : 0; }

  void set_error(const std::string &msg) {
    has_error = true;
    error = msg;
  }

  static constexpr long long state_started = 1;
  static constexpr long long state_stopped = 0;
  static constexpr long long state_unreadable = -1;
  static constexpr long long state_hung = -2;
  static constexpr long long state_unknown = -10;

  long long get_state_i() const {
    if (unreadable) return state_unreadable;
    if (hung) return state_hung;
    if (started) return state_started;
    return state_stopped;
  }

  static long long parse_state(const std::string &s) {
    if (s == "started") return state_started;
    if (s == "stopped") return state_stopped;
    if (s == "hung") return state_hung;
    if (s == "unreadable") return state_unreadable;
    return state_unknown;
  }

  process_info &operator+=(const process_info &other) {
    // State: the total row reports "started" if any aggregated process is
    // started and "hung" if any is hung (hung takes precedence in
    // get_state_s/get_state_i). Without this the total is always "stopped",
    // which trips the default critical filter `state = 'stopped'`.
    if (other.started) started = true;
    if (other.hung.get()) hung = true;

    // Handles
    handleCount += other.handleCount.get();
    gdiHandleCount += other.gdiHandleCount.get();
    userHandleCount += other.userHandleCount.get();
    threadCount += other.threadCount.get();

    // System-wide totals are constants shared by every process; carry them onto
    // the aggregate row (don't sum) so working_set_pct/pagefile_pct stay correct.
    if (other.total_physical_memory) total_physical_memory = other.total_physical_memory;
    if (other.total_pagefile) total_pagefile = other.total_pagefile;

    // TImes
    creation_time += other.creation_time.get();
    kernel_time += other.kernel_time.get();
    user_time += other.user_time.get();
    kernel_time_raw += other.kernel_time_raw;
    user_time_raw += other.user_time_raw;

    // IO Counters
    readOperationCount += other.readOperationCount.get();
    writeOperationCount += other.writeOperationCount.get();
    otherOperationCount += other.otherOperationCount.get();
    readTransferCount += other.readTransferCount.get();
    writeTransferCount += other.writeTransferCount.get();
    otherTransferCount += other.otherTransferCount.get();

    // Mem Counters
    PeakVirtualSize += other.PeakVirtualSize.get();
    VirtualSize += other.VirtualSize.get();
    PageFaultCount += other.PageFaultCount.get();
    PeakWorkingSetSize += other.PeakWorkingSetSize.get();
    WorkingSetSize += other.WorkingSetSize.get();
    QuotaPeakPagedPoolUsage += other.QuotaPeakPagedPoolUsage.get();
    QuotaPagedPoolUsage += other.QuotaPagedPoolUsage.get();
    QuotaPeakNonPagedPoolUsage += other.QuotaPeakNonPagedPoolUsage.get();
    QuotaNonPagedPoolUsage += other.QuotaNonPagedPoolUsage.get();
    PageFileUsage += other.PageFileUsage.get();
    PeakPageFileUsage += other.PeakPageFileUsage.get();

    return *this;
  }

  process_info &operator-=(const process_info &other) {
    // Handles
    handleCount -= other.handleCount.get();
    gdiHandleCount -= other.gdiHandleCount.get();
    userHandleCount -= other.userHandleCount.get();
    // threadCount is intentionally NOT subtracted: like the CPU counters it is an
    // absolute gauge, so the delta path keeps the current snapshot's value.

    // Times. The CPU time counters are intentionally NOT subtracted here: the
    // per-process CPU delta (together with its guards against PID reuse) is
    // computed in make_cpu_delta() directly from the two raw snapshots.
    // Subtracting the raw counters here as well would underflow the unsigned
    // values whenever a PID is recycled mid-interval (the headline bug behind
    // the absurd percentages such as "cpu=72562484370%").
    creation_time -= other.creation_time.get();

    // IO Counters
    readOperationCount -= other.readOperationCount.get();
    writeOperationCount -= other.writeOperationCount.get();
    otherOperationCount -= other.otherOperationCount.get();
    readTransferCount -= other.readTransferCount.get();
    writeTransferCount -= other.writeTransferCount.get();
    otherTransferCount -= other.otherTransferCount.get();

    // Mem Counters
    PeakVirtualSize -= other.PeakVirtualSize.get();
    VirtualSize -= other.VirtualSize.get();
    PageFaultCount -= other.PageFaultCount.get();
    PeakWorkingSetSize -= other.PeakWorkingSetSize.get();
    WorkingSetSize -= other.WorkingSetSize.get();
    QuotaPeakPagedPoolUsage -= other.QuotaPeakPagedPoolUsage.get();
    QuotaPagedPoolUsage -= other.QuotaPagedPoolUsage.get();
    QuotaPeakNonPagedPoolUsage -= other.QuotaPeakNonPagedPoolUsage.get();
    QuotaNonPagedPoolUsage -= other.QuotaNonPagedPoolUsage.get();
    PageFileUsage -= other.PageFileUsage.get();
    PeakPageFileUsage -= other.PeakPageFileUsage.get();

    return *this;
  }

  // Compute `part * 100 / whole` rounded to the nearest whole percent rather
  // than truncated. Plain truncation made every sub-1% process read 0% (and
  // biased busier processes low), which is why repeated runs so often showed
  // "everything at 0". Rounding keeps small-but-real usage visible.
  static unsigned long long to_percent(const unsigned long long part, const unsigned long long whole) {
    if (whole == 0) return 0;
    return (part * 100ull + whole / 2ull) / whole;
  }

  // Turn this snapshot into per-process CPU usage over the sampling interval,
  // using `previous` as the earlier snapshot of the SAME process. `sys_kernel`
  // and `sys_user` are the system-wide kernel/user time consumed over the same
  // interval; their sum is the total CPU capacity. On Windows GetSystemTimes()
  // already folds idle time into the kernel figure, so idle must NOT be added a
  // second time (doing so inflated the denominator and halved every result).
  //
  // On success kernel_time, user_time and total_time hold whole-percent CPU
  // usage and the function returns true. It returns false - and zeroes the
  // fields - when the delta is not meaningful and the caller should drop the
  // process: when a raw counter moved backwards (the PID was recycled to a
  // different process mid-interval) or when no capacity was measured.
  bool make_cpu_delta(const process_info &previous, const unsigned long long sys_kernel, const unsigned long long sys_user) {
    const unsigned long long capacity = sys_kernel + sys_user;
    if (capacity == 0 || kernel_time_raw < previous.kernel_time_raw || user_time_raw < previous.user_time_raw) {
      kernel_time = 0;
      user_time = 0;
      total_time = 0;
      return false;
    }
    const unsigned long long kernel_delta = kernel_time_raw - previous.kernel_time_raw;
    const unsigned long long user_delta = user_time_raw - previous.user_time_raw;
    kernel_time = to_percent(kernel_delta, capacity);
    user_time = to_percent(user_delta, capacity);
    total_time = to_percent(kernel_delta + user_delta, capacity);
    return true;
  }
  static std::shared_ptr<process_info> get_total();

  std::string to_string() const { return exe.get(); }
};

struct error_reporter {
  virtual ~error_reporter() = default;
  virtual void report_error(std::string error) = 0;
  virtual void report_warning(std::string error) = 0;
  virtual void report_debug(std::string error) = 0;
};

typedef std::list<process_info> process_list;
process_list enumerate_processes(bool ignore_unreadable = false, bool find_16bit = false, bool deep_scan = true, error_reporter *error_interface = nullptr,
                                 unsigned int buffer_size = DEFAULT_BUFFER_SIZE);
process_list enumerate_processes_delta(bool ignore_unreadable, error_reporter *error_interface);

void enable_token_privilege(LPTSTR privilege, bool enable);
}  // namespace win_list_processes

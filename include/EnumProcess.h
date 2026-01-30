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

#pragma once

#include <boost/shared_ptr.hpp>
#include <list>
#include <str/format.hpp>
#include <str/xtos.hpp>
#include <string>
#include <utility>
#include <win/windows.hpp>

#define DEFAULT_BUFFER_SIZE 4096

namespace process_helper {
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
    // Handles
    handleCount += other.handleCount.get();
    gdiHandleCount += other.gdiHandleCount.get();
    userHandleCount += other.userHandleCount.get();

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

    // TImes
    creation_time -= other.creation_time.get();
    kernel_time -= other.kernel_time.get();
    user_time -= other.user_time.get();
    kernel_time_raw -= other.kernel_time_raw;
    user_time_raw -= other.user_time_raw;

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

  void make_cpu_delta(const unsigned long long kernel, const unsigned long long user, const unsigned long long total) {
    if (kernel > 0) kernel_time = kernel_time_raw * 100ull / kernel;
    if (user > 0) user_time = user_time_raw * 100ull / user;
    if (total > 0) total_time = (kernel_time_raw + user_time_raw) * 100ull / total;
  }
  static boost::shared_ptr<process_info> get_total();

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
}  // namespace process_helper

// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_process.h"

#include <dirent.h>
#include <pwd.h>
#include <unistd.h>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <ctime>
#include <fstream>
#include <map>
#include <mutex>
#include <nscapi/protobuf/functions_convert.hpp>
#include <nscapi/protobuf/functions_exec.hpp>
#include <nscapi/protobuf/functions_query.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <set>
#include <sstream>
#include <str/xtos.hpp>

namespace po = boost::program_options;

using namespace parsers::where;

namespace check_proc {

namespace check_proc_filter {

// Case-insensitive string comparison for process names
struct CaseBlindCompare {
  bool operator()(const std::string &a, const std::string &b) const { return boost::ilexicographical_compare(a, b); }
};

node_type parse_state(std::shared_ptr<filter_obj> object, evaluation_context context, node_type subject) {
  return factory::create_int(filter_obj::parse_state(subject->get_string_value(context)));
}

node_type parse_proc_state(std::shared_ptr<filter_obj> object, evaluation_context context, node_type subject) {
  return factory::create_int(filter_obj::parse_proc_state(subject->get_string_value(context)));
}

filter_obj_handler::filter_obj_handler() {
  static const value_type type_custom_state = type_custom_int_1;
  static const value_type type_custom_proc_state = type_custom_int_2;

  registry_.add_string_var("filename", &filter_obj::get_filename, "Name of process (with path)")
      .add_string_var("exe", &filter_obj::get_exe, "The name of the executable")
      .add_string_var("error", &filter_obj::get_error, "Any error messages associated with fetching info")
      .add_string_var("command_line", &filter_obj::get_command_line, "Command line of process")
      .add_string_var("username", &filter_obj::get_username, "Process owner user name (empty unless resolve-owner=true)");

  registry_.add_int_var("pid", &filter_obj::get_pid, "Process id")
      .add_int_var("ppid", &filter_obj::get_ppid, "Parent process id")
      .add_int_var("uid", &filter_obj::get_uid,
                   "Real uid of the process owner (/proc/<pid>/status on Linux, the BSD process info on macOS); -1 when not known (the synthetic "
                   "'not found' and total rows)")
      .add_int_var("started", &filter_obj::get_started, "Process is started")
      .add_int_var("stopped", &filter_obj::get_stopped, "Process is stopped")
      .add_int_var("state", type_custom_state, &filter_obj::get_state_i,
                   "Cross-platform state verdict: started or stopped ('running' is accepted as a synonym for started in expressions; the rendered value "
                   "stays 'started')")
      .add_int_var("proc_state", type_custom_proc_state, &filter_obj::get_proc_state_i,
                   "Raw scheduler state (the letter ps prints in its STAT column): running, sleeping, disk_sleep, zombie, stopped, tracing_stop, "
                   "dead, idle, parked or unknown. macOS has running, sleeping, zombie and stopped; running vs sleeping needs the process's task "
                   "info, so it is unknown for other users' processes when the agent is not root");

  registry_.add_human_string("state", &filter_obj::get_state_s, "The current state (started, stopped)");
  registry_.add_human_string("proc_state", &filter_obj::get_proc_state_s, "The raw process scheduler state");

  // Memory counters. Perfdata mirrors the Windows check_process: working set and
  // virtual size are emitted as scaled bytes, page faults as a plain counter.
  // They are optional because macOS only hands task info to a process's owner
  // and root: an unreadable counter renders "unknown", never satisfies a
  // threshold and emits no perf data (see filter_obj::has_task_info).
  // clang-format off
  registry_.add_optional_int_var("virtual", parsers::where::type_size, [](auto obj) { return obj->task_value(obj->virtual_size); }, "unknown",
                                 "Virtual size in bytes")
      .add_scaled_byte_perf("", " v_size")
      .add_optional_int_var("working_set", parsers::where::type_size, [](auto obj) { return obj->task_value(obj->working_set); }, "unknown",
                            "Working set (RSS) in bytes")
      .add_scaled_byte_perf("", " ws_size")
      .add_optional_int_var("page_faults", [](auto obj) { return obj->task_value(obj->page_faults); }, "unknown",
                            "Page fault count (major faults on Linux, pageins on macOS)")
      .add_int_perf("", "", " pf_count");

  // Peak memory counters (VmPeak / VmHWM) and the page_fault alias mirror the
  // Windows keyword names so queries are portable both ways. macOS keeps no
  // per-process peaks, so there they are unknown.
  registry_.add_optional_int_var("peak_virtual", parsers::where::type_size, [](auto obj) { return obj->peak_value(obj->peak_virtual_size); }, "unknown",
                                 "Peak virtual size in bytes (unknown on macOS)")
      .add_scaled_byte_perf("", " pv_size")
      .add_optional_int_var("peak_working_set", parsers::where::type_size, [](auto obj) { return obj->peak_value(obj->peak_working_set); }, "unknown",
                            "Peak working set in bytes (unknown on macOS)")
      .add_scaled_byte_perf("", " pws_size")
      .add_optional_int_var("page_fault", [](auto obj) { return obj->task_value(obj->page_faults); }, "unknown", "Page fault count")
      .add_int_perf("", "", " pf_count");

  // `rss` is a straight alias for `working_set`, matching the Windows
  // check_process keyword set so the same expression works on both platforms.
  registry_.add_optional_int_var("rss", parsers::where::type_size, [](auto obj) { return obj->task_value(obj->working_set); }, "unknown",
                                 "Resident set size in bytes; alias for working_set, matching the Windows keyword set (g,m,k,b)")
      .add_scaled_byte_perf("", " rss");
  // clang-format on

  registry_.add_human_string_context("virtual", &filter_obj::get_virtual_size_human, "")
      .add_human_string_context("working_set", &filter_obj::get_working_set_human, "");
  registry_.add_human_string_context("rss", &filter_obj::get_working_set_human, "");
  registry_.add_human_string_context("peak_virtual", &filter_obj::get_peak_virtual_size_human, "")
      .add_human_string_context("peak_working_set", &filter_obj::get_peak_working_set_human, "");

  // Time counters. Perfdata mirrors Windows (no UOM): cumulative CPU seconds
  // normally, whole percentages of total CPU with delta=true. creation is the
  // process start time as an absolute timestamp (date type, like Windows).
  registry_.add_int_legacy()("creation", parsers::where::type_date, [](auto obj, auto context) { return obj->get_creation_time(); }, "Creation time")
      .add_perf("", "", " creation")("elapsed", [](auto obj, auto context) { return obj->get_elapsed(); },
                                     "Wall-clock seconds since the process started (0 when not known)")
      .add_perf("s", "", " elapsed");
  // clang-format off
  registry_.add_optional_int_var("user", [](auto obj) { return obj->task_value(obj->user_time); }, "unknown", "User time in seconds")
      .add_int_perf("", "", " user")
      .add_optional_int_var("kernel", [](auto obj) { return obj->task_value(obj->kernel_time); }, "unknown", "Kernel time in seconds")
      .add_int_perf("", "", " kernel")
      .add_optional_int_var("time", [](auto obj) { return obj->task_value(obj->total_time); }, "unknown", "User-kernel time in seconds")
      .add_int_perf("", "", " total");
  // clang-format on

  registry_.add_converter(type_custom_state, &parse_state);
  registry_.add_converter(type_custom_proc_state, &parse_proc_state);
}

bool parse_proc_pid_stat(const std::string &line, proc_stat_data &data) {
  // Format: pid (comm) state ppid pgrp session tty_nr tpgid flags minflt
  // cminflt majflt cmajflt utime stime cutime cstime priority nice
  // num_threads itrealvalue starttime ...
  // comm may contain spaces, parentheses and even ") (" sequences, so it is
  // delimited by the first '(' and the LAST ')'.
  const std::size_t open_pos = line.find('(');
  const std::size_t close_pos = line.rfind(')');
  if (open_pos == std::string::npos || close_pos == std::string::npos || close_pos < open_pos || close_pos + 2 >= line.length()) return false;
  data.comm = line.substr(open_pos + 1, close_pos - open_pos - 1);

  std::istringstream iss(line.substr(close_pos + 2));
  char state = '?';
  int ppid, pgrp, session, tty_nr, tpgid;
  unsigned long long flags, minflt, cminflt, majflt, cmajflt, utime, stime;
  long long cutime, cstime, priority, nice, num_threads, itrealvalue;
  unsigned long long starttime;
  iss >> state >> ppid >> pgrp >> session >> tty_nr >> tpgid >> flags >> minflt >> cminflt >> majflt >> cmajflt >> utime >> stime >> cutime >> cstime >>
      priority >> nice >> num_threads >> itrealvalue >> starttime;
  if (iss.fail()) return false;

  data.state = state;
  data.ppid = ppid;
  data.major_faults = majflt;
  data.utime_jiffies = utime;
  data.stime_jiffies = stime;
  data.starttime_jiffies = starttime;
  return true;
}

bool parse_proc_status_bytes(const std::string &content, const std::string &key, unsigned long long &bytes) {
  const std::string prefix = key + ":";
  std::istringstream iss(content);
  std::string line;
  while (std::getline(iss, line)) {
    if (line.compare(0, prefix.length(), prefix) == 0) {
      std::istringstream value(line.substr(prefix.length()));
      unsigned long long kb = 0;
      value >> kb;
      if (value.fail()) return false;
      bytes = kb * 1024ull;
      return true;
    }
  }
  return false;
}

bool parse_proc_status_uid(const std::string &content, long long &uid) {
  std::istringstream iss(content);
  std::string line;
  while (std::getline(iss, line)) {
    if (line.compare(0, 4, "Uid:") == 0) {
      // "Uid:\t<real>\t<effective>\t<saved>\t<filesystem>" — the real uid is
      // the process owner.
      std::istringstream value(line.substr(4));
      long long real_uid = -1;
      value >> real_uid;
      if (value.fail() || real_uid < 0) return false;
      uid = real_uid;
      return true;
    }
  }
  return false;
}

std::string lookup_username(const long long uid) {
  if (uid < 0) return "";

  // Memoised: a host has a handful of distinct uids but can have thousands of
  // processes, and each miss is an NSS lookup. Negative results are cached too
  // so a deleted uid is not retried once per process. The cache lives for the
  // lifetime of the agent, so a uid renamed underneath us stays stale until
  // restart — an acceptable trade for not hammering NSS.
  static std::mutex cache_mutex;
  static std::map<long long, std::string> cache;
  {
    std::lock_guard<std::mutex> lock(cache_mutex);
    const auto it = cache.find(uid);
    if (it != cache.end()) return it->second;
  }

  std::string name;
  long bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
  if (bufsize <= 0) bufsize = 16384;
  std::vector<char> buffer(static_cast<std::size_t>(bufsize));
  struct passwd pwd;
  struct passwd *result = nullptr;
  if (getpwuid_r(static_cast<uid_t>(uid), &pwd, buffer.data(), buffer.size(), &result) == 0 && result != nullptr && result->pw_name != nullptr) {
    name = result->pw_name;
  }

  std::lock_guard<std::mutex> lock(cache_mutex);
  cache[uid] = name;
  return name;
}

bool parse_proc_stat_cpu_total(const std::string &content, unsigned long long &total_jiffies) {
  std::istringstream iss(content);
  std::string line;
  while (std::getline(iss, line)) {
    // The aggregate line is "cpu  ..." (per-core lines are "cpu0 ..." etc).
    if (line.compare(0, 4, "cpu ") == 0) {
      // Fields: user nice system idle iowait irq softirq steal guest guest_nice.
      // guest/guest_nice are already included in user/nice, so only the first
      // eight fields are summed to avoid double counting.
      std::istringstream fields(line.substr(4));
      unsigned long long total = 0;
      unsigned long long value = 0;
      int count = 0;
      while (count < 8 && (fields >> value)) {
        total += value;
        ++count;
      }
      if (count == 0) return false;
      total_jiffies = total;
      return true;
    }
  }
  return false;
}

bool parse_proc_stat_btime(const std::string &content, unsigned long long &btime) {
  std::istringstream iss(content);
  std::string line;
  while (std::getline(iss, line)) {
    if (line.compare(0, 6, "btime ") == 0) {
      std::istringstream value(line.substr(6));
      unsigned long long v = 0;
      value >> v;
      if (value.fail()) return false;
      btime = v;
      return true;
    }
  }
  return false;
}

// Delta mode (mirrors the Windows enumerate_processes_delta): snapshot the
// processes and the total system capacity, sleep one second, snapshot again
// and turn the per-process CPU counters into whole percentages of total CPU.
// The two capacity reads bracket both process snapshots so the numerator
// (per-process CPU time) and denominator (system capacity) cover the same
// wall-clock window.
std::vector<filter_obj> enumerate_processes_delta(bool resolve_owner) {
  unsigned long long capacity_start = 0;
  const bool have_start = read_cpu_capacity(capacity_start);

  // Only the second (reported) snapshot needs owner names; the first is used
  // purely for the CPU counters it carries.
  const std::vector<filter_obj> first = enumerate_processes();
  usleep(1000 * 1000);
  std::vector<filter_obj> second = enumerate_processes(resolve_owner);

  unsigned long long capacity_end = 0;
  const bool have_end = read_cpu_capacity(capacity_end);
  const unsigned long long capacity = (have_start && have_end && capacity_end > capacity_start) ? capacity_end - capacity_start : 0;

  std::map<int, const filter_obj *> previous;
  for (const filter_obj &info : first) {
    previous[info.pid] = &info;
  }

  std::vector<filter_obj> ret;
  for (filter_obj &info : second) {
    // Processes that started or died during the window are dropped (like on
    // Windows): there is no full one-second sample for them.
    const auto it = previous.find(info.pid);
    if (it == previous.end()) continue;
    // Guard against PID reuse: the start time identifies the PID incarnation;
    // if it changed the two snapshots describe different programs and any
    // delta between them is meaningless.
    if (it->second->start_time_jiffies != info.start_time_jiffies) continue;
    // make_cpu_delta also rejects counters that moved backwards; drop the
    // process rather than emit a bogus percentage.
    if (!info.make_cpu_delta(*it->second, capacity)) continue;
    ret.push_back(info);
  }
  return ret;
}

}  // namespace check_proc_filter

void check_process(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  // `fetch-only` short-circuits the filter machinery and emits one line per
  // process in `<<<ps>>>` format: (user,vsz_kb,rss_kb,cputime,pid) cmdline.
  // user is left empty; user_time + kernel_time are returned in seconds.
  for (int i = 0; i < request.arguments_size(); i++) {
    const std::string &a = request.arguments(i);
    if (a == "fetch-only" || a == "--fetch-only") {
      std::string body;
      const std::vector<check_proc_filter::filter_obj> procs = check_proc_filter::enumerate_processes();
      for (const check_proc_filter::filter_obj &p : procs) {
        const long long vsz_kb = static_cast<long long>(p.virtual_size / 1024);
        const long long rss_kb = static_cast<long long>(p.working_set / 1024);
        const long long cputime = static_cast<long long>(p.user_time + p.kernel_time);
        const std::string cmd = p.command_line.empty() ? p.exe : p.command_line;
        if (!body.empty()) body += "\n";
        body += "(," + str::xtos(vsz_kb) + "," + str::xtos(rss_kb) + "," + str::xtos(cputime) + "," + str::xtos(p.pid) + ") " + cmd;
      }
      nscapi::protobuf::functions::append_simple_query_response_payload(response, "check_process", NSCAPI::query_return_codes::returnOK, body, "");
      return;
    }
  }

  typedef check_proc_filter::filter filter_type;
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  std::vector<std::string> processes;
  bool delta_scan = false;
  bool total = false;
  bool resolve_owner = false;

  filter_type filter;
  filter_helper.add_filter_option("state != 'unreadable'");
  filter_helper.add_warn_option("state not in ('started')");
  filter_helper.add_crit_option("state = 'stopped'", "count = 0");

  filter_helper.add_options(filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${problem_list}", "${exe}=${state}", "${exe}", "UNKNOWN: No processes found", "%(status): all processes are ok.");

  // clang-format off
  filter_helper.get_desc().add_options()
    ("process", po::value<std::vector<std::string>>(&processes), "The process to check, set this to * to check all processes")
    ("delta", po::value<bool>(&delta_scan), "Measure CPU usage as a delta over a one second interval.\nThe check samples process and system CPU times, sleeps for one second, then samples again. With delta=true the 'time' (and 'kernel'/'user') fields report the process CPU usage during that second as a whole percentage of total CPU, instead of cumulative CPU seconds.")
    ("total", po::value<bool>(&total)->implicit_value(true)->default_value(false), "Include the total of all matching processes")
    ("resolve-owner", po::value<bool>(&resolve_owner)->implicit_value(true)->default_value(false),
        "Populate the username keyword with the process owner's user name. Off by default: the lookup goes through NSS and can block for seconds when it is backed by a remote directory (LDAP/SSSD). The numeric uid keyword is always populated and needs no flag.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  if (processes.empty()) {
    processes.emplace_back("*");
  }

  if (!filter_helper.build_filter(filter)) return;

  std::set<std::string, check_proc_filter::CaseBlindCompare> procs;
  bool all = false;
  for (const std::string &process : processes) {
    if (process == "*")
      all = true;
    else if (procs.count(process) == 0)
      procs.insert(process);
  }

  std::shared_ptr<check_proc_filter::filter_obj> total_obj;
  if (total) {
    total_obj = std::shared_ptr<check_proc_filter::filter_obj>(new check_proc_filter::filter_obj());
    total_obj->exe = "total";
  }

  std::vector<std::string> matched;
  std::vector<check_proc_filter::filter_obj> process_list =
      delta_scan ? check_proc_filter::enumerate_processes_delta(resolve_owner) : check_proc_filter::enumerate_processes(resolve_owner);

  for (const check_proc_filter::filter_obj &info : process_list) {
    bool wanted = procs.count(info.exe) > 0;
    if (all || wanted) {
      std::shared_ptr<check_proc_filter::filter_obj> record(new check_proc_filter::filter_obj(info));
      modern_filter::match_result ret = filter.match(record);
      if (total_obj && ret.matched_filter) {
        *total_obj += *record;
      }
    }
    if (wanted) {
      matched.push_back(info.exe);
    }
  }

  // Remove matched processes from the wanted list
  for (const std::string &proc : matched) {
    procs.erase(proc);
  }

  // For any process that wasn't found, create a "stopped" entry
  for (const std::string &proc : procs) {
    std::shared_ptr<check_proc_filter::filter_obj> record(new check_proc_filter::filter_obj(proc));
    record->started = false;
    modern_filter::match_result ret = filter.match(record);
    if (total_obj && ret.matched_filter) {
      *total_obj += *record;
    }
  }

  if (total_obj) {
    filter.match(total_obj);
  }

  filter_helper.post_process(filter);
}

}  // namespace check_proc

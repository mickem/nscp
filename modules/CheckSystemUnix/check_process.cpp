#include "check_process.h"

#include <dirent.h>
#include <unistd.h>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <fstream>
#include <map>
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

filter_obj_handler::filter_obj_handler() {
  static const value_type type_custom_state = type_custom_int_1;

  registry_.add_string_var("filename", &filter_obj::get_filename, "Name of process (with path)")
      .add_string_var("exe", &filter_obj::get_exe, "The name of the executable")
      .add_string_var("error", &filter_obj::get_error, "Any error messages associated with fetching info")
      .add_string_var("command_line", &filter_obj::get_command_line, "Command line of process");

  registry_.add_int_var("pid", &filter_obj::get_pid, "Process id")
      .add_int_var("started", &filter_obj::get_started, "Process is started")
      .add_int_var("stopped", &filter_obj::get_stopped, "Process is stopped")
      .add_int_var("state", type_custom_state, &filter_obj::get_state_i, "The current state (started, stopped, hung)");

  registry_.add_human_string("state", &filter_obj::get_state_s, "The current state (started, stopped, hung)");

  // Memory counters. Perfdata mirrors the Windows check_process: working set and
  // virtual size are emitted as scaled bytes, page faults as a plain counter.
  registry_.add_int_legacy()("virtual", parsers::where::type_size, [](auto obj, auto context) { return obj->get_virtual_size(); }, "Virtual size in bytes")
      .add_scaled_byte(std::string(""), " v_size")("working_set", parsers::where::type_size,
                                                    [](auto obj, auto context) { return obj->get_working_set(); }, "Working set (RSS) in bytes")
      .add_scaled_byte(std::string(""), " ws_size")("page_faults", [](auto obj, auto context) { return obj->get_page_faults(); }, "Page fault count")
      .add_perf("", "", " pf_count");

  // Peak memory counters (VmPeak / VmHWM) and the page_fault alias mirror the
  // Windows keyword names so queries are portable both ways.
  registry_.add_int_legacy()("peak_virtual", parsers::where::type_size, [](auto obj, auto context) { return obj->get_peak_virtual_size(); },
                             "Peak virtual size in bytes")
      .add_scaled_byte(std::string(""), " pv_size")("peak_working_set", parsers::where::type_size,
                                                    [](auto obj, auto context) { return obj->get_peak_working_set(); }, "Peak working set in bytes")
      .add_scaled_byte(std::string(""), " pws_size")("page_fault", [](auto obj, auto context) { return obj->get_page_faults(); }, "Page fault count")
      .add_perf("", "", " pf_count");

  registry_.add_human_string("virtual", &filter_obj::get_virtual_size_human, "").add_human_string("working_set", &filter_obj::get_working_set_human, "");
  registry_.add_human_string("peak_virtual", &filter_obj::get_peak_virtual_size_human, "")
      .add_human_string("peak_working_set", &filter_obj::get_peak_working_set_human, "");

  // Time counters. Perfdata mirrors Windows (no UOM): cumulative CPU seconds
  // normally, whole percentages of total CPU with delta=true. creation is the
  // process start time as an absolute timestamp (date type, like Windows).
  registry_.add_int_legacy()("creation", parsers::where::type_date, [](auto obj, auto context) { return obj->get_creation_time(); }, "Creation time")
      .add_perf("", "", " creation")("user", [](auto obj, auto context) { return obj->get_user_time(); }, "User time in seconds")
      .add_perf("", "", " user")("kernel", [](auto obj, auto context) { return obj->get_kernel_time(); }, "Kernel time in seconds")
      .add_perf("", "", " kernel")("time", [](auto obj, auto context) { return obj->get_total_time(); }, "User-kernel time in seconds")
      .add_perf("", "", " total");

  registry_.add_converter(type_custom_state, &parse_state);
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
filter_obj read_process_info(int pid) {
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

        info.user_time_raw = stat_data.utime_jiffies;
        info.kernel_time_raw = stat_data.stime_jiffies;
        info.start_time_jiffies = stat_data.starttime_jiffies;

        // Convert jiffies to seconds (typically 100 Hz = USER_HZ)
        long ticks_per_sec = sysconf(_SC_CLK_TCK);
        if (ticks_per_sec > 0) {
          info.user_time = stat_data.utime_jiffies / ticks_per_sec;
          info.kernel_time = stat_data.stime_jiffies / ticks_per_sec;
          info.creation_time = get_boot_time() + stat_data.starttime_jiffies / ticks_per_sec;
        }
        info.total_time = info.user_time + info.kernel_time;

        info.page_faults = stat_data.major_faults;
      }
    }
  } catch (...) {
    info.error = "Cannot read stat";
  }

  // Read /proc/[pid]/status for the peak memory counters. Kernel threads have
  // no Vm* entries; the peaks then stay 0.
  try {
    std::ifstream status_file(proc_path + "/status");
    if (status_file.is_open()) {
      std::stringstream ss;
      ss << status_file.rdbuf();
      const std::string content = ss.str();
      parse_proc_status_bytes(content, "VmPeak", info.peak_virtual_size);
      parse_proc_status_bytes(content, "VmHWM", info.peak_working_set);
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
std::vector<filter_obj> enumerate_processes() {
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
        filter_obj info = read_process_info(pid);
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

// Delta mode (mirrors the Windows enumerate_processes_delta): snapshot the
// processes and the total system jiffies, sleep one second, snapshot again and
// turn the per-process CPU counters into whole percentages of total CPU. The
// two /proc/stat reads bracket both process snapshots so the numerator
// (per-process jiffies) and denominator (system jiffies) cover the same
// wall-clock window.
std::vector<filter_obj> enumerate_processes_delta() {
  unsigned long long capacity_start = 0;
  const bool have_start = parse_proc_stat_cpu_total(read_file("/proc/stat"), capacity_start);

  const std::vector<filter_obj> first = enumerate_processes();
  usleep(1000 * 1000);
  std::vector<filter_obj> second = enumerate_processes();

  unsigned long long capacity_end = 0;
  const bool have_end = parse_proc_stat_cpu_total(read_file("/proc/stat"), capacity_end);
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
      delta_scan ? check_proc_filter::enumerate_processes_delta() : check_proc_filter::enumerate_processes();

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

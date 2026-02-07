#include "check_process.h"

#include <dirent.h>
#include <unistd.h>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <fstream>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <set>
#include <sstream>

namespace po = boost::program_options;

using namespace parsers::where;

namespace check_proc {

namespace check_proc_filter {

// Case-insensitive string comparison for process names
struct CaseBlindCompare {
  bool operator()(const std::string &a, const std::string &b) const { return boost::ilexicographical_compare(a, b); }
};

node_type parse_state(boost::shared_ptr<filter_obj> object, evaluation_context context, node_type subject) {
  return factory::create_int(filter_obj::parse_state(subject->get_string_value(context)));
}

filter_obj_handler::filter_obj_handler() {
  static const value_type type_custom_state = type_custom_int_1;

  registry_.add_string("filename", &filter_obj::get_filename, "Name of process (with path)")
      .add_string("exe", &filter_obj::get_exe, "The name of the executable")
      .add_string("error", &filter_obj::get_error, "Any error messages associated with fetching info")
      .add_string("command_line", &filter_obj::get_command_line, "Command line of process");

  registry_.add_int_x("pid", &filter_obj::get_pid, "Process id")
      .add_int_x("started", &filter_obj::get_started, "Process is started")
      .add_int_x("stopped", &filter_obj::get_stopped, "Process is stopped")
      .add_int_x("state", type_custom_state, &filter_obj::get_state_i, "The current state (started, stopped, hung)");

  // Memory counters
  registry_.add_int()(
      "virtual", [](auto obj, auto context) { return obj->get_virtual_size(); }, "Virtual size in bytes")(
      "working_set", [](auto obj, auto context) { return obj->get_working_set(); }, "Working set (RSS) in bytes")(
      "page_faults", [](auto obj, auto context) { return obj->get_page_faults(); }, "Page fault count");

  registry_.add_human_string("virtual", &filter_obj::get_virtual_size_human, "").add_human_string("working_set", &filter_obj::get_working_set_human, "");

  // Time counters
  registry_.add_int()(
      "user", [](auto obj, auto context) { return obj->get_user_time(); }, "User time in seconds")(
      "kernel", [](auto obj, auto context) { return obj->get_kernel_time(); }, "Kernel time in seconds")(
      "time", [](auto obj, auto context) { return obj->get_total_time(); }, "User + kernel time in seconds");

  registry_.add_converter()(type_custom_state, &parse_state);
}

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

      // Format: pid (comm) state ppid pgrp session tty_nr tpgid flags minflt cminflt majflt cmajflt utime stime ...
      // Find the closing paren to skip the comm field (which may contain spaces)
      std::size_t paren_pos = line.rfind(')');
      if (paren_pos != std::string::npos && paren_pos + 2 < line.length()) {
        std::istringstream iss(line.substr(paren_pos + 2));
        char state;
        int ppid, pgrp, session, tty_nr, tpgid;
        unsigned int flags;
        unsigned long minflt, cminflt, majflt, cmajflt, utime, stime;

        iss >> state >> ppid >> pgrp >> session >> tty_nr >> tpgid >> flags >> minflt >> cminflt >> majflt >> cmajflt >> utime >> stime;

        // State: R=running, S=sleeping, D=disk sleep, Z=zombie, T=stopped, t=tracing stop, X=dead
        info.started = (state == 'R' || state == 'S' || state == 'D');

        // Convert jiffies to seconds (typically 100 Hz = USER_HZ)
        long ticks_per_sec = sysconf(_SC_CLK_TCK);
        if (ticks_per_sec > 0) {
          info.user_time = utime / ticks_per_sec;
          info.kernel_time = stime / ticks_per_sec;
        }

        info.page_faults = majflt;
      }
    }
  } catch (...) {
    info.error = "Cannot read stat";
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

}  // namespace check_proc_filter

void check_process(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  typedef check_proc_filter::filter filter_type;
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  std::vector<std::string> processes;
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
    ("total", po::bool_switch(&total), "Include the total of all matching processes")
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

  std::vector<std::string> matched;
  std::vector<check_proc_filter::filter_obj> process_list = check_proc_filter::enumerate_processes();

  for (const check_proc_filter::filter_obj &info : process_list) {
    bool wanted = procs.count(info.exe) > 0;
    if (all || wanted) {
      boost::shared_ptr<check_proc_filter::filter_obj> record(new check_proc_filter::filter_obj(info));
      filter.match(record);
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
  boost::shared_ptr<check_proc_filter::filter_obj> total_obj;
  if (total) {
    total_obj = boost::shared_ptr<check_proc_filter::filter_obj>(new check_proc_filter::filter_obj());
    total_obj->exe = "total";
  }

  for (const std::string &proc : procs) {
    boost::shared_ptr<check_proc_filter::filter_obj> record(new check_proc_filter::filter_obj(proc));
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

// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_process.hpp"

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <nscapi/protobuf/functions_query.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/filter/realtime_helper.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <parsers/where/node.hpp>
#include <str/xtos.hpp>
#include <string>
#include <win/processes.hpp>

namespace check_proc_filter {
typedef win_list_processes::process_info filter_obj;

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

parsers::where::node_type parse_state(std::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
  return parsers::where::factory::create_int(filter_obj::parse_state(subject->get_string_value(context)));
}

filter_obj_handler::filter_obj_handler() {
  static const parsers::where::value_type type_custom_state = parsers::where::type_custom_int_1;
  static const parsers::where::value_type type_custom_start_type = parsers::where::type_custom_int_2;

  registry_.add_string_var("filename", &filter_obj::get_filename, "Name of process (with path)")
      .add_string_var("exe", &filter_obj::get_exe, "The name of the executable")
      .add_string_var("error", &filter_obj::get_error, "Any error messages associated with fetching info")
      .add_string_var("command_line", &filter_obj::get_command_line, "Command line of process (not always available)")
      .add_string_var("username", &filter_obj::get_username, "Process owner as DOMAIN\\name (empty for processes whose token cannot be read)")
      .add_string_var("uid", &filter_obj::get_uid, "Process owner SID (the Windows analogue of a Unix uid)")
      .add_string_var("legacy_state", &filter_obj::get_legacy_state_s, "Get process status (for legacy use via check_nt only)");
  registry_.add_int_var("pid", &filter_obj::get_pid, "Process id")
      .add_int_var("started", parsers::where::type_bool, &filter_obj::get_started, "Process is started")
      .add_int_var("hung", parsers::where::type_bool, &filter_obj::get_hung, "Process is hung")
      .add_int_var("stopped", parsers::where::type_bool, &filter_obj::get_stopped, "Process is stopped")
      .add_int_var("new", parsers::where::type_bool, &filter_obj::get_is_new, "Process is new (can inly be used for real-time filters)");
  // clang-format off
  registry_.add_int_legacy()
    ("handles", [](auto obj, auto context) {return obj->get_handleCount(); }, "Number of handles").add_perf("", "", " handle count")
    ("gdi_handles", [](auto obj, auto context) {return obj->get_gdiHandleCount(); }, "Number of handles").add_perf("", "", " GDI handle count")
    ("user_handles", [](auto obj, auto context) {return obj->get_userHandleCount(); }, "Number of handles").add_perf("", "", " USER handle count")
    ("thread_count", [](auto obj, auto context) {return obj->get_threadCount(); }, "Number of threads").add_perf("", "", " threads")
    ("peak_virtual", parsers::where::type_size, [](auto obj, auto context) {return obj->get_PeakVirtualSize(); }, "Peak virtual size in bytes (g,m,k,b)").add_scaled_byte(std::string(""), " pv_size")
    ("virtual", parsers::where::type_size,[](auto obj, auto context) {return obj->get_VirtualSize(); }, "Virtual size in bytes (g,m,k,b)").add_scaled_byte(std::string(""), " v_size")
    ("page_fault", [](auto obj, auto context) {return obj->get_PageFaultCount(); }, "Page fault count").add_perf("", "", " pf_count")
    ("peak_working_set", parsers::where::type_size, [](auto obj, auto context) {return obj->get_PeakWorkingSetSize(); }, "Peak working set in bytes (g,m,k,b)").add_scaled_byte(std::string(""), " pws_size")
    ("working_set", parsers::where::type_size, [](auto obj, auto context) {return obj->get_WorkingSetSize(); }, "Working set in bytes (g,m,k,b)").add_scaled_byte(std::string(""), " ws_size")
    ("rss", parsers::where::type_size, [](auto obj, auto context) {return obj->get_WorkingSetSize(); }, "Resident set size; alias for working_set (g,m,k,b)").add_scaled_byte(std::string(""), " rss")
    ("working_set_pct", [](auto obj, auto context) {return obj->get_working_set_pct(); }, "Working set as a percentage of total physical RAM").add_perf("%", "", " ws_pct")
    // 			("qouta", parsers::where::type_size, [](auto obj, auto context) {return obj->get_QuotaPeakPagedPoolUsage, _1), "TODO").add_scaled_byte(std::string(""), " v_size")
    // 			("virtual_size", parsers::where::type_size, [](auto obj, auto context) {return obj->get_QuotaPagedPoolUsage, _1), "TODO").add_scaled_byte(std::string(""), " v_size")
    // 			("virtual_size", parsers::where::type_size, [](auto obj, auto context) {return obj->get_QuotaPeakNonPagedPoolUsage, _1), "TODO").add_scaled_byte(std::string(""), " v_size")
    // 			("virtual_size", parsers::where::type_size, [](auto obj, auto context) {return obj->get_QuotaNonPagedPoolUsage, _1), "TODO").add_scaled_byte(std::string(""), " v_size")
    ("peak_pagefile", parsers::where::type_size,[](auto obj, auto context) {return obj->get_PageFileUsage(); }, "Page file usage in bytes (g,m,k,b)").add_scaled_byte(std::string(""), " ppf_use")
    ("pagefile", parsers::where::type_size, [](auto obj, auto context) {return obj->get_PeakPageFileUsage(); }, "Peak page file use in bytes (g,m,k,b)").add_scaled_byte(std::string(""), " pf_use")
    ("pagefile_pct", [](auto obj, auto context) {return obj->get_pagefile_pct(); }, "Page file usage as a percentage of the system commit limit").add_perf("%", "", " pf_pct")

    ("creation", parsers::where::type_date, [](auto obj, auto context) {return obj->get_creation_time(); }, "Creation time").add_perf("", "", " creation")
    ("kernel", [](auto obj, auto context) {return obj->get_kernel_time(); }, "Kernel CPU time: cumulative seconds, or % of total CPU with delta=true").add_perf("", "", " kernel")
    ("user", [](auto obj, auto context) {return obj->get_user_time(); }, "User CPU time: cumulative seconds, or % of total CPU with delta=true").add_perf("", "", " user")
    ("time", [](auto obj, auto context) {return obj->get_total_time(); }, "User+kernel CPU time: cumulative seconds, or % of total CPU with delta=true").add_perf("", "", " total")

    ("state", type_custom_state, [](auto obj, auto context) {return obj->get_state_i(); }, "The current state (started, stopped hung)").add_perf("", "", " state")
    ;
  // clang-format on

  registry_.add_human_string("state", &filter_obj::get_state_s, "The current state (started, stopped hung)");
  registry_.add_human_string("peak_virtual", &filter_obj::get_PeakVirtualSize_human, "");
  registry_.add_human_string("virtual", &filter_obj::get_VirtualSize_human, "");
  registry_.add_human_string("peak_working_set", &filter_obj::get_PeakWorkingSetSize_human, "");
  registry_.add_human_string("working_set", &filter_obj::get_WorkingSetSize_human, "");
  registry_.add_human_string("rss", &filter_obj::get_WorkingSetSize_human, "");
  registry_.add_human_string("peak_pagefile", &filter_obj::get_PageFileUsage_human, "");
  registry_.add_human_string("pagefile", &filter_obj::get_PeakPageFileUsage_human, "");

  registry_.add_converter(type_custom_state, &parse_state);
}

}  // namespace check_proc_filter

class NSC_error : public win_list_processes::error_reporter {
  void report_error(std::string error) { NSC_LOG_ERROR(error); }
  void report_warning(std::string error) { NSC_LOG_MESSAGE(error); }
  void report_debug(std::string error) { NSC_DEBUG_MSG_STD(error); }
};

namespace process_checks {

namespace realtime {

struct transient_data {
  win_list_processes::process_list list;

  transient_data(win_list_processes::process_list list) : list(list) {}
  const transient_data &operator=(const transient_data &other) {
    list = other.list;
    return *this;
  }

  std::string to_string() const { return str::xtos(list.size()) + " processes"; }
};

struct runtime_data {
  typedef check_proc_filter::filter filter_type;
  typedef std::shared_ptr<transient_data> transient_data_type;

  std::list<std::string> checks;
  bool has_check_all;

  runtime_data() : has_check_all(false) {}
  void boot() {}
  void touch(boost::posix_time::ptime now) {}
  bool has_changed(transient_data_type) const { return true; }
  modern_filter::match_result process_item(filter_type &filter, transient_data_type);
  void add(const std::string &data);
};

struct proc_filter_helper_wrapper {
  typedef parsers::where::realtime_filter_helper<runtime_data, filters::proc::filter_config_object> proc_filter_helper;
  proc_filter_helper helper;

  proc_filter_helper_wrapper(nscapi::core_wrapper *core, int plugin_id) : helper(core, plugin_id) {}
};

void runtime_data::add(const std::string &data) {
  if (data == "*") {
    has_check_all = true;
  } else {
    checks.push_back(data);
  }
}

bool process_name_matches_any(const std::list<std::string> &names, const std::string &candidate) {
  return std::any_of(names.begin(), names.end(), [&candidate](const std::string &name) { return boost::algorithm::iequals(name, candidate); });
}

modern_filter::match_result runtime_data::process_item(filter_type &filter, transient_data_type data) {
  modern_filter::match_result ret;

  if (has_check_all) {
    for (const win_list_processes::process_info &info : data->list) {
      std::shared_ptr<win_list_processes::process_info> record(new win_list_processes::process_info(info));
      ret.append(filter.match(record));
    }
  } else {
    for (const win_list_processes::process_info &info : data->list) {
      if (process_name_matches_any(checks, info.exe.get())) {
        std::shared_ptr<win_list_processes::process_info> record(new win_list_processes::process_info(info));
        ret.append(filter.match(record));
      }
    }
  }
  return ret;
}

helper::helper(nscapi::core_wrapper *core, int plugin_id) : proc_helper(new proc_filter_helper_wrapper(core, plugin_id)) {}

void helper::add_obj(std::shared_ptr<filters::proc::filter_config_object> object) {
  runtime_data data;
  for (const std::string &d : object->data) {
    data.add(d);
  }
  proc_helper->helper.add_item(object, data, "system.process");
}

void helper::boot() { proc_helper->helper.touch_all(); }

void helper::check() {
  NSC_error err;

  runtime_data::transient_data_type data(new transient_data(win_list_processes::enumerate_processes_delta(true, &err)));
  for (win_list_processes::process_info &i : data->list) {
    if (known_processes_.find(i.exe.get()) == known_processes_.end()) {
      i.is_new = true;
      known_processes_.emplace(i.exe.get());
    }
  }
  proc_helper->helper.process_items(data);
}

std::set<std::string> helper::check_shared() {
  NSC_error err;
  std::set<std::string> running_exes;

  runtime_data::transient_data_type data(new transient_data(win_list_processes::enumerate_processes_delta(true, &err)));
  for (win_list_processes::process_info &i : data->list) {
    std::string exe_lower = boost::algorithm::to_lower_copy(i.exe.get());
    if (!exe_lower.empty()) {
      running_exes.insert(exe_lower);
    }
    if (known_processes_.find(i.exe.get()) == known_processes_.end()) {
      i.is_new = true;
      known_processes_.emplace(i.exe.get());
    }
  }
  proc_helper->helper.process_items(data);

  return running_exes;
}

std::map<std::string, long long> helper::get_counts() const { return proc_helper->helper.get_counts(); }

}  // namespace realtime

struct CaseBlindCompare {
  bool operator()(const std::string &a, const std::string &b) const { return _stricmp(a.c_str(), b.c_str()) < 0; }
};

namespace active {

namespace po = boost::program_options;

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response, const cpu_delta_map &cpu_deltas,
           bool cpu_collector_enabled) {
  // `fetch-only` short-circuits the filter machinery and emits one line per
  // process in `<<<ps>>>` format: (user,vsz_kb,rss_kb,cputime,pid) cmdline.
  // user is left empty here because process_info does not carry it.
  for (int i = 0; i < request.arguments_size(); i++) {
    const std::string &a = request.arguments(i);
    if (a == "fetch-only" || a == "--fetch-only") {
      NSC_error err;
      win_list_processes::process_list list = win_list_processes::enumerate_processes(false, false, true, &err);
      std::string body;
      for (const win_list_processes::process_info &p : list) {
        const long long vsz_kb = static_cast<long long>(p.VirtualSize.get() / 1024);
        const long long rss_kb = static_cast<long long>(p.WorkingSetSize.get() / 1024);
        const long long cputime = static_cast<long long>(p.total_time.get());
        const std::string cmd = p.command_line.get().empty() ? p.exe.get() : p.command_line.get();
        if (!body.empty()) body += "\n";
        body += "(," + str::xtos(vsz_kb) + "," + str::xtos(rss_kb) + "," + str::xtos(cputime) + "," + str::xtos(p.pid.get()) + ") " + cmd;
      }
      nscapi::protobuf::functions::append_simple_query_response_payload(response, "check_process", NSCAPI::query_return_codes::returnOK, body, "");
      return;
    }
  }

  typedef check_proc_filter::filter filter_type;
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  std::vector<std::string> processes;
  bool deep_scan = true;
  bool vdm_scan = false;
  bool unreadable_scan = true;
  bool delta_scan = false;
  bool total = false;
  bool resolve_owner = false;

  NSC_error err;
  filter_type filter;
  filter_helper.add_filter_option("state != 'unreadable'");
  filter_helper.add_warn_option("state not in ('started')");
  filter_helper.add_crit_option("state = 'stopped'", "count = 0");

  filter_helper.add_options(filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${problem_list}", "${exe}=${state}", "${exe}", "UNKNOWN: No processes found", "%(status): all processes are ok.");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("process", po::value<std::vector<std::string>>(&processes), "The service to check, set this to * to check all services")
    ("scan-info", po::value<bool>(&deep_scan), "If all process metrics should be fetched (otherwise only status is fetched)")
    ("scan-16bit", po::value<bool>(&vdm_scan), "If 16bit processes should be included")
    ("delta", po::value<bool>(&delta_scan)->implicit_value(true)->default_value(false), "Report CPU usage as a percentage of total CPU instead of cumulative seconds.\nWith delta=true the 'time' (and 'kernel'/'user') fields report the process CPU usage over a one second window as a whole percentage of total CPU. The reading is taken from the CheckSystem background collector (no per-check sleep), so it requires 'process cpu = true' under [/settings/system/windows]; without that the check returns UNKNOWN telling you to enable it.")
    ("scan-unreadable", po::value<bool>(&unreadable_scan), "If unreadable processes should be included (will not have information)")
    ("total", po::value<bool>(&total)->implicit_value(true)->default_value(false), "Include the total of all matching files")
    ("resolve-owner", po::value<bool>(&resolve_owner)->implicit_value(true)->default_value(false),
        "Populate the username/uid keywords with the process owner. Off by default: resolving the owner name can block for seconds on domain / Azure-AD accounts.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  // delta=true reports CPU as a percentage sourced from the background
  // collector. It is only meaningful when that collector is running, so fail
  // fast (rather than silently reporting cumulative seconds or zeroes) when the
  // flag is off, naming the setting to switch on. Cumulative CPU seconds (delta
  // omitted) need no collector and are unaffected.
  if (delta_scan && !cpu_collector_enabled) {
    return nscapi::protobuf::functions::set_response_bad(
        *response,
        "check_process delta=true requires per-process CPU sampling, which is disabled by default. Set 'process cpu = true' under "
        "[/settings/system/windows] and restart to report kernel/user/time as CPU% over a 1 second window.");
  }

  if (processes.empty()) {
    processes.emplace_back("*");
  }
  if (!filter_helper.build_filter(filter)) return;

  // delta reporting matches the collector's CPU% to each process by PID and
  // creation time; the creation time is only captured during a deep scan, so a
  // delta request implies scan-info regardless of how it was set.
  if (delta_scan) deep_scan = true;

  // Overlay the collector's CPU% (guaranteed enabled past the guard above) onto
  // a freshly enumerated process, matched by PID + creation time. A process
  // missing from the snapshot (just started or already gone) reports 0% rather
  // than leaking its absolute cumulative-seconds value as though it were a
  // percentage.
  auto apply_cpu_delta = [&](win_list_processes::process_info &rec) {
    if (!delta_scan) return;
    long long kpct = 0, upct = 0, tpct = 0;
    const auto it = cpu_deltas.find(static_cast<long long>(rec.get_pid()));
    if (it != cpu_deltas.end() && it->second.creation_time == static_cast<unsigned long long>(rec.get_creation_time())) {
      kpct = it->second.kernel_pct;
      upct = it->second.user_pct;
      tpct = it->second.total_pct;
    }
    rec.kernel_time = kpct;
    rec.user_time = upct;
    rec.total_time = tpct;
  };

  std::set<std::string, CaseBlindCompare> procs;
  bool all = false;
  for (const std::string &process : processes) {
    if (process == "*")
      all = true;
    else if (procs.count(process) == 0)
      procs.insert(process);
  }

  std::shared_ptr<win_list_processes::process_info> total_obj;
  if (total) total_obj = win_list_processes::process_info::get_total();

  std::vector<std::string> matched;
  win_list_processes::process_list list =
      win_list_processes::enumerate_processes(!unreadable_scan, vdm_scan, deep_scan, &err, DEFAULT_BUFFER_SIZE, resolve_owner);
  for (const win_list_processes::process_info &info : list) {
    bool wanted = procs.count(info.exe);
    if (all || wanted) {
      std::shared_ptr<win_list_processes::process_info> record(new win_list_processes::process_info(info));
      apply_cpu_delta(*record);
      modern_filter::match_result ret = filter.match(record);
      if (total_obj && ret.matched_filter) total_obj->operator+=(*record);
    }
    if (wanted) {
      matched.push_back(info.exe);
    }
  }
  for (const std::string &proc : matched) {
    procs.erase(proc);
  }

  for (const std::string &proc : procs) {
    std::shared_ptr<win_list_processes::process_info> record(new win_list_processes::process_info(proc));
    apply_cpu_delta(*record);
    modern_filter::match_result ret = filter.match(record);
    if (total_obj && ret.matched_filter) total_obj->operator+=(*record);
  }
  if (total_obj) filter.match(total_obj);
  filter_helper.post_process(filter);
}
}  // namespace active
}  // namespace process_checks

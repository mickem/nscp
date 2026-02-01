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

#include "check_process.hpp"

#include <nscapi/nscapi_protobuf_functions.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/filter/realtime_helper.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <parsers/where/node.hpp>
#include <string>
#include <win/processes.hpp>

namespace check_proc_filter {
typedef win_list_processes::process_info filter_obj;

typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

parsers::where::node_type parse_state(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
  return parsers::where::factory::create_int(filter_obj::parse_state(subject->get_string_value(context)));
}

filter_obj_handler::filter_obj_handler() {
  static const parsers::where::value_type type_custom_state = parsers::where::type_custom_int_1;
  static const parsers::where::value_type type_custom_start_type = parsers::where::type_custom_int_2;

  registry_.add_string("filename", &filter_obj::get_filename, "Name of process (with path)")
      .add_string("exe", &filter_obj::get_exe, "The name of the executable")
      .add_string("error", &filter_obj::get_error, "Any error messages associated with fetching info")
      .add_string("command_line", &filter_obj::get_command_line, "Command line of process (not always available)")
      .add_string("legacy_state", &filter_obj::get_legacy_state_s, "Get process status (for legacy use via check_nt only)");
  registry_.add_int_x("pid", &filter_obj::get_pid, "Process id")
      .add_int_x("started", parsers::where::type_bool, &filter_obj::get_started, "Process is started")
      .add_int_x("hung", parsers::where::type_bool, &filter_obj::get_hung, "Process is hung")
      .add_int_x("stopped", parsers::where::type_bool, &filter_obj::get_stopped, "Process is stopped")
      .add_int_x("new", parsers::where::type_bool, &filter_obj::get_is_new, "Process is new (can inly be used for real-time filters)");
  // clang-format off
  registry_.add_int()
    ("handles", [](auto obj, auto context) {return obj->get_handleCount(); }, "Number of handles").add_perf("", "", " handle count")
    ("gdi_handles", [](auto obj, auto context) {return obj->get_gdiHandleCount(); }, "Number of handles").add_perf("", "", " GDI handle count")
    ("user_handles", [](auto obj, auto context) {return obj->get_userHandleCount(); }, "Number of handles").add_perf("", "", " USER handle count")
    ("peak_virtual", parsers::where::type_size, [](auto obj, auto context) {return obj->get_PeakVirtualSize(); }, "Peak virtual size in bytes (g,m,k,b)").add_scaled_byte(std::string(""), " pv_size")
    ("virtual", parsers::where::type_size,[](auto obj, auto context) {return obj->get_VirtualSize(); }, "Virtual size in bytes (g,m,k,b)").add_scaled_byte(std::string(""), " v_size")
    ("page_fault", [](auto obj, auto context) {return obj->get_PageFaultCount(); }, "Page fault count").add_perf("", "", " pf_count")
    ("peak_working_set", parsers::where::type_size, [](auto obj, auto context) {return obj->get_PeakWorkingSetSize(); }, "Peak working set in bytes (g,m,k,b)").add_scaled_byte(std::string(""), " pws_size")
    ("working_set", parsers::where::type_size, [](auto obj, auto context) {return obj->get_WorkingSetSize(); }, "Working set in bytes (g,m,k,b)").add_scaled_byte(std::string(""), " ws_size")
    // 			("qouta", parsers::where::type_size, [](auto obj, auto context) {return obj->get_QuotaPeakPagedPoolUsage, _1), "TODO").add_scaled_byte(std::string(""), " v_size")
    // 			("virtual_size", parsers::where::type_size, [](auto obj, auto context) {return obj->get_QuotaPagedPoolUsage, _1), "TODO").add_scaled_byte(std::string(""), " v_size")
    // 			("virtual_size", parsers::where::type_size, [](auto obj, auto context) {return obj->get_QuotaPeakNonPagedPoolUsage, _1), "TODO").add_scaled_byte(std::string(""), " v_size")
    // 			("virtual_size", parsers::where::type_size, [](auto obj, auto context) {return obj->get_QuotaNonPagedPoolUsage, _1), "TODO").add_scaled_byte(std::string(""), " v_size")
    ("peak_pagefile", parsers::where::type_size,[](auto obj, auto context) {return obj->get_PageFileUsage(); }, "Page file usage in bytes (g,m,k,b)").add_scaled_byte(std::string(""), " ppf_use")
    ("pagefile", parsers::where::type_size, [](auto obj, auto context) {return obj->get_PeakPageFileUsage(); }, "Peak page file use in bytes (g,m,k,b)").add_scaled_byte(std::string(""), " pf_use")

    ("creation", parsers::where::type_date, [](auto obj, auto context) {return obj->get_creation_time(); }, "Creation time").add_perf("", "", " creation")
    ("kernel", [](auto obj, auto context) {return obj->get_kernel_time(); }, "Kernel time in seconds").add_perf("", "", " kernel")
    ("user", [](auto obj, auto context) {return obj->get_user_time(); }, "User time in seconds").add_perf("", "", " user")
    ("time", [](auto obj, auto context) {return obj->get_total_time(); }, "User-kernel time in seconds").add_perf("", "", " total")

    ("state", type_custom_state, [](auto obj, auto context) {return obj->get_state_i(); }, "The current state (started, stopped hung)").add_perf("", "", " state")
    ;
  // clang-format on

  registry_.add_human_string("state", &filter_obj::get_state_s, "The current state (started, stopped hung)");
  registry_.add_human_string("peak_virtual", &filter_obj::get_PeakVirtualSize_human, "");
  registry_.add_human_string("virtual", &filter_obj::get_VirtualSize_human, "");
  registry_.add_human_string("peak_working_set", &filter_obj::get_PeakWorkingSetSize_human, "");
  registry_.add_human_string("working_set", &filter_obj::get_WorkingSetSize_human, "");
  registry_.add_human_string("peak_pagefile", &filter_obj::get_PageFileUsage_human, "");
  registry_.add_human_string("pagefile", &filter_obj::get_PeakPageFileUsage_human, "");

  registry_.add_converter()(type_custom_state, &parse_state);
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
  typedef boost::shared_ptr<transient_data> transient_data_type;

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

modern_filter::match_result runtime_data::process_item(filter_type &filter, transient_data_type data) {
  modern_filter::match_result ret;

  if (has_check_all) {
    for (const win_list_processes::process_info &info : data->list) {
      boost::shared_ptr<win_list_processes::process_info> record(new win_list_processes::process_info(info));
      ret.append(filter.match(record));
    }
  } else {
    for (const win_list_processes::process_info &info : data->list) {
      bool found = (std::find(checks.begin(), checks.end(), info.exe.get()) != checks.end());
      if (found) {
        boost::shared_ptr<win_list_processes::process_info> record(new win_list_processes::process_info(info));
        ret.append(filter.match(record));
      }
    }
  }
  return ret;
}

helper::helper(nscapi::core_wrapper *core, int plugin_id) : proc_helper(new proc_filter_helper_wrapper(core, plugin_id)) {}

void helper::add_obj(boost::shared_ptr<filters::proc::filter_config_object> object) {
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

}  // namespace realtime

struct CaseBlindCompare {
  bool operator()(const std::string &a, const std::string &b) const { return _stricmp(a.c_str(), b.c_str()) < 0; }
};

namespace active {

namespace po = boost::program_options;

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  typedef check_proc_filter::filter filter_type;
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  std::vector<std::string> processes;
  bool deep_scan = true;
  bool vdm_scan = false;
  bool unreadable_scan = true;
  bool delta_scan = false;
  bool total = false;

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
    ("delta", po::value<bool>(&delta_scan), "Calculate delta over one elapsed second.\nThis call will measure values and then sleep for 2 second and then measure again calculating deltas.")
    ("scan-unreadable", po::value<bool>(&unreadable_scan), "If unreadable processes should be included (will not have information)")
    ("total", po::bool_switch(&total), "Include the total of all matching files")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  if (processes.empty()) {
    processes.emplace_back("*");
  }
  if (!filter_helper.build_filter(filter)) return;

  std::set<std::string, CaseBlindCompare> procs;
  bool all = false;
  for (const std::string &process : processes) {
    if (process == "*")
      all = true;
    else if (procs.count(process) == 0)
      procs.insert(process);
  }

  std::vector<std::string> matched;
  win_list_processes::process_list list = delta_scan ? win_list_processes::enumerate_processes_delta(!unreadable_scan, &err)
                                                     : win_list_processes::enumerate_processes(!unreadable_scan, vdm_scan, deep_scan, &err);
  for (const win_list_processes::process_info &info : list) {
    bool wanted = procs.count(info.exe);
    if (all || wanted) {
      boost::shared_ptr<win_list_processes::process_info> record(new win_list_processes::process_info(info));
      filter.match(record);
    }
    if (wanted) {
      matched.push_back(info.exe);
    }
  }
  for (const std::string &proc : matched) {
    procs.erase(proc);
  }

  boost::shared_ptr<win_list_processes::process_info> total_obj;
  if (total) total_obj = win_list_processes::process_info::get_total();

  for (const std::string &proc : procs) {
    boost::shared_ptr<win_list_processes::process_info> record(new win_list_processes::process_info(proc));
    modern_filter::match_result ret = filter.match(record);
    if (total_obj && ret.matched_filter) total_obj->operator+=(*record);
  }
  if (total_obj) filter.match(total_obj);
  filter_helper.post_process(filter);
}
}  // namespace active
}  // namespace process_checks

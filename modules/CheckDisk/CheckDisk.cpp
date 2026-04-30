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

#include "CheckDisk.h"

#include <compat.hpp>
#include <file_helpers.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_metrics_helper.hpp>
#include <nscapi/nscapi_plugin_wrapper.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/settings/helper.hpp>
#include <nscapi/settings/proxy.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/helpers.hpp>

#include "check_disk_health.hpp"
#include "check_drive.hpp"
#include "file_finder.hpp"
#include "filter.hpp"

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

CheckDisk::CheckDisk() : show_errors_(false) {}

bool CheckDisk::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
  collector_.reset(new collector_thread(get_core(), get_id()));

  sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
  settings.set_alias("disk", alias);

  // clang-format off
  settings.alias().add_key_to_settings()
    .add_string("disable", sh::string_key(&collector_->disable_, ""),
        "Disable automatic checks",
        "A comma separated list of checks to disable in the collector: disk_io, disk_free. "
        "Please note disabling these will mean part of NSClient++ will no longer function as expected.", true)
    ;
  // clang-format on
  settings.register_all();
  settings.notify();

  if (mode == NSCAPI::normalStart) {
    collector_->start();
  }
  return true;
}

bool CheckDisk::unloadModule() {
  if (collector_) {
    collector_->stop();
  }
  return true;
}

void CheckDisk::check_disk_io(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  if (!collector_) {
    nscapi::protobuf::functions::set_response_bad(*response, "Collector not started");
    return;
  }
  try {
    disk_io_check::check::check_disk_io(request, response, collector_->get_disk_io());
  } catch (const std::exception &e) {
    nscapi::protobuf::functions::set_response_bad(*response, "Failed to get disk I/O data: " + std::string(e.what()));
  }
}

void CheckDisk::check_disk_health(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  if (!collector_) {
    nscapi::protobuf::functions::set_response_bad(*response, "Collector not started");
    return;
  }
  try {
    auto data = disk_health_check::join(collector_->get_disk_io(), collector_->get_disk_free());
    disk_health_check::check::check_disk_health(request, response, data);
  } catch (const std::exception &e) {
    nscapi::protobuf::functions::set_response_bad(*response, "Failed to get disk health data: " + std::string(e.what()));
  }
}

void CheckDisk::fetchMetrics(PB::Metrics::MetricsMessage::Response *response) {
  if (!collector_) return;

  using namespace nscapi::metrics;

  PB::Metrics::MetricsBundle *bundle = response->add_bundles();
  bundle->set_key("disk");

  const auto disks = collector_->get_disk_io();
  if (!disks.empty()) {
    PB::Metrics::MetricsBundle *section = bundle->add_children();
    section->set_key("io");
    for (const disk_io_check::disks_type::value_type &v : disks) {
      v.build_metrics(section);
    }
  }

  const auto drives = collector_->get_disk_free();
  if (!drives.empty()) {
    PB::Metrics::MetricsBundle *section = bundle->add_children();
    section->set_key("free");
    for (const disk_free_check::drives_type::value_type &v : drives) {
      v.build_metrics(section);
    }
  }
}

void CheckDisk::checkDriveSize(PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  boost::program_options::options_description desc;

  std::vector<std::string> times;
  std::vector<std::string> types;
  std::string perf_unit;
  nscapi::program_options::add_help(desc);
  // clang-format off
  desc.add_options()
    ("CheckAll", po::value<std::string>()->implicit_value("true"), "Checks all drives.")
    ("CheckAllOthers", po::value<std::string>()->implicit_value("true"), "Checks all drives turns the drive option into an exclude option.")
    ("Drive", po::value<std::vector<std::string>>(&times), "The drives to check")
    ("FilterType", po::value<std::vector<std::string>>(&types), "The type of drives to check fixed, remote, cdrom, ramdisk, removable")
    ("perf-unit", po::value<std::string>(&perf_unit), "Force performance data to use a given unit prevents scaling which can cause problems over time in some graphing solutions.")
    ;
  // clang-format on
  compat::addShowAll(desc);
  compat::addAllNumeric(desc);
  compat::addAllNumeric(desc, "Free");
  compat::addAllNumeric(desc, "Used");

  boost::program_options::variables_map vm;
  std::vector<std::string> extra;
  if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response, true, extra)) return;
  std::string warn, crit;

  request.clear_arguments();
  compat::matchFirstNumeric(vm, "used", "free", warn, crit);
  compat::matchFirstNumeric(vm, "used", "used", warn, crit, "Used");
  compat::matchFirstNumeric(vm, "free", "free", warn, crit, "Free");
  compat::inline_addarg(request, warn);
  compat::inline_addarg(request, crit);
  if (vm.count("CheckAll")) request.add_arguments("drive=*");
  bool exclude = false;
  if (vm.count("CheckAllOthers")) {
    request.add_arguments("drive=*");
    exclude = true;
  }
  if (!perf_unit.empty()) request.add_arguments("perf-config=free(unit:" + perf_unit + ")used(unit:" + perf_unit + ")");
  request.add_arguments("detail-syntax=%(drive): Total: %(size) - Used: %(used) (%(used_pct)%) - Free: %(free) (%(free_pct)%)");
  compat::matchShowAll(vm, request);
  std::string keyword = exclude ? "exclude=" : "drive=";
  for (const std::string &t : times) {
    request.add_arguments(keyword + t);
  }
  for (const std::string &t : extra) {
    request.add_arguments(keyword + t);
  }

  if (!types.empty()) {
    std::string type_list = "";
    for (const std::string &s : types) {
      if (!type_list.empty()) type_list += ", ";
      type_list += "'" + s + "'";
    }
    request.add_arguments("filter=type in (" + type_list + ")");
  }
  compat::log_args(request);
  check_drive::check(request, response);
}

void CheckDisk::check_drivesize(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_drive::check(request, response);
}

void CheckDisk::checkFiles(PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  boost::program_options::options_description desc;

  std::vector<std::string> times;
  std::vector<std::string> types;
  std::string syntax = "${filename}";
  std::string master_syntax = "${list}";
  std::string path;
  std::string pattern;
  std::string filter;
  std::string warn2;
  std::string crit2;
  bool debug = false;
  int maxDepth = 0;
  nscapi::program_options::add_help(desc);
  // clang-format off
  desc.add_options()
    ("syntax", po::value<std::string>(&syntax), "Syntax for individual items (detail-syntax).")
    ("master-syntax", po::value<std::string>(&master_syntax), "Syntax for top syntax (top-syntax).")
    ("path", po::value<std::string>(&path), "The file or path to check")
    ("pattern", po::value<std::string>(&pattern), "Deprecated and ignored")
    ("alias", po::value<std::string>(), "Deprecated and ignored")
    ("debug", po::bool_switch(&debug), "Debug")
    ("max-dir-depth", po::value<int>(&maxDepth), "The maximum level to recurse")
    ("filter", po::value<std::string>(&filter), "The filter to use when including files in the check")
    ("warn", po::value<std::string>(&warn2), "Deprecated and ignored")
    ("crit", po::value<std::string>(&crit2), "Deprecated and ignored")
    ;
  // clang-format on
  compat::addAllNumeric(desc);

  boost::program_options::variables_map vm;
  if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response)) return;
  std::string warn, crit;

  request.clear_arguments();
  compat::matchFirstNumeric(vm, "count", "count", warn, crit);
  if (!warn.empty() && !warn2.empty()) {
    NSC_LOG_ERROR("Duplicate warnings not supported.");
  } else if (!warn2.empty()) {
    boost::replace_all(warn2, ":", " ");
    warn = "warn=count " + warn2;
  }
  if (!crit.empty() && !crit2.empty()) {
    NSC_LOG_ERROR("Duplicate warnings not supported.");
  } else if (!crit2.empty()) {
    boost::replace_all(crit2, ":", " ");
    crit = "crit=count " + crit2;
  }
  compat::inline_addarg(request, warn);
  compat::inline_addarg(request, crit);
  compat::inline_addarg(request, "filter=", filter);
  compat::inline_addarg(request, "pattern=", pattern);

  boost::replace_all(syntax, "%filename%", "%(filename)");
  boost::replace_all(syntax, "%size%", "%(size)");
  boost::replace_all(syntax, "%write%", "%(written)");
  compat::inline_addarg(request, "detail-syntax=", syntax);

  boost::replace_all(master_syntax, "%list%", "%(list)");
  boost::replace_all(master_syntax, "%count%", "%(count)");
  boost::replace_all(master_syntax, "%total%", "%(total)");
  compat::inline_addarg(request, "top-syntax=", master_syntax);
  compat::inline_addarg(request, "path=", path);
  if (debug) request.add_arguments("debug");
  if (maxDepth > 0) request.add_arguments("max-depth=" + str::xtos(maxDepth));
  // Legacy CheckFiles historically returned OK for "0 matching files" when
  // the user only supplied MaxWarn/MaxCrit thresholds (i.e. an empty result
  // set was simply "below the warning threshold"). Modern check_files now
  // defaults empty-state to "unknown", which surfaces in legacy callers as
  // a spurious UNKNOWN status (issue #717). Preserve the legacy behaviour
  // by defaulting empty-state to ok for this shim.
  request.add_arguments("empty-state=ok");
  compat::log_args(request);
  check_files(request, response);
}

void CheckDisk::check_files(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<file_filter::filter> filter_helper(request, response, data);
  std::vector<std::string> file_list;
  std::string files_string;
  std::string mode;
  file_finder::scanner_context context;
  context.max_depth = -1;
  std::string total;

  file_filter::filter filter;
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${problem_count}/${count} files (${problem_list})", "${name}", "${name}", "No files found",
                           "%(status): All %(count) files are ok");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("path", po::value<std::vector<std::string> >(&file_list), "The path to search for files under.\nNotice that specifying multiple path will create an aggregate set you will not check each path individually."
        "In other words if one path contains an error the entire check will result in error.")
    ("file", po::value<std::vector<std::string> >(&file_list), "Alias for path.")
    ("paths", po::value<std::string>(&files_string), "A comma separated list of paths to scan")
    ("pattern", po::value<std::string>(&context.pattern)->default_value("*.*"), "The pattern of files to search for (works like a filter but is faster and can be combined with a filter).")
    ("max-depth", po::value<int>(&context.max_depth), "Maximum depth to recurse")
    ("total", po::value(&total)->implicit_value("filter"), "Include the total of either (filter) all files matching the filter or (all) all files regardless of the filter")
  ;
  // clang-format on

  context.now = parsers::where::constants::get_now();

  if (!filter_helper.parse_options()) return;

  if (!files_string.empty()) boost::split(file_list, files_string, boost::is_any_of(","));

  if (file_list.empty()) return nscapi::protobuf::functions::set_response_bad(*response, "No path specified");

  if (!filter_helper.build_filter(filter)) return;

  boost::shared_ptr<file_filter::filter_obj> total_obj;
  if (!total.empty()) total_obj = file_filter::filter_obj::get_total(context.now);

  for (const std::string &path : file_list) {
    file_finder::recursive_scan(filter, context, path, total_obj, total == "all");
  }
  if (!context.missing_paths.empty()) {
    // One or more user-supplied top-level paths could not be opened. Surface
    // this as UNKNOWN with the offending path(s) so operators see a clear
    // error instead of a misleading OK / "No files found" (issue #613).
    std::string joined;
    for (const std::string &p : context.missing_paths) {
      if (!joined.empty()) joined += ", ";
      joined += p;
    }
    return nscapi::protobuf::functions::set_response_bad(*response, "Path was not found: " + joined);
  }
  if (total_obj) {
    filter.match(total_obj);
  }
  filter_helper.post_process(filter);
}

void CheckDisk::check_single_file(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<file_filter::filter> filter_helper(request, response, data);
  std::string file_path;

  file_filter::filter filter;
  // No "empty" state: a single-file check either has the file (and runs the
  // filter) or it does not (and we return UNKNOWN with a useful message).
  // Default to UNKNOWN if no thresholds are set, so check_single_file <file>
  // by itself confirms the file exists rather than being a silent OK.
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "ok");
  filter_helper.add_syntax(
      "%(status): %(filename) (size=%(size), age=%(age))",
      "%(filename) (size=%(size), age=%(age))",
      "%(filename)",
      // The "empty" syntaxes below are unreachable for check_single_file (we
      // always either fail with UNKNOWN or feed exactly one object to the
      // filter) but cli_helper requires non-empty defaults.
      "No file inspected",
      "%(status): %(filename) is ok");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("file", po::value<std::string>(&file_path), "The file to check.")
    ("path", po::value<std::string>(&file_path), "Alias for file.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  if (file_path.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, "No file specified (use file=<path>)");
  }

  if (!filter_helper.build_filter(filter)) return;

  long long now = parsers::where::constants::get_now();
  boost::shared_ptr<file_filter::filter_obj> info = file_finder::stat_single_file(file_path, now);
  if (!info) {
    return nscapi::protobuf::functions::set_response_bad(*response, "File not found: " + file_path);
  }

  filter.match(info);
  filter_helper.post_process(filter);
}
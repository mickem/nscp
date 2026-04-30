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
#include "check_files.hpp"
#include "check_single_file.hpp"
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
  check_files_command::check(request, response);
}

void CheckDisk::check_single_file(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_single_file_command::check(request, response);
}
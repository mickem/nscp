// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "CheckDisk.h"

#ifdef WIN32
#include <windows.h>
#endif

#include <algorithm>
#include <cctype>
#include <limits>
#include <compat.hpp>
#include <nscapi/macros.hpp>
#include <str/format.hpp>
#include <str/utf8.hpp>
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
#include "check_disk_write.hpp"
#include "check_drive.hpp"
#include "check_files.hpp"
#include "check_mount.hpp"
#include "check_single_file.hpp"
#include "check_shadowcopy.hpp"
#include "check_share.hpp"
#include "check_storagepool.hpp"
#include "check_uncpath.hpp"
#include "file_finder.hpp"
#include "filter.hpp"

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

CheckDisk::CheckDisk() : show_errors_(false) {}

namespace {
// The collector hands out an immutable, shared snapshot (null before it has
// ticked, or when no collector is running); the check wants a plain map. The
// caller keeps the snapshot alive for the duration of the check.
const check_drive::trend_map &deref_trends(const collector_thread::trend_snapshot &snapshot) {
  static const check_drive::trend_map empty;
  return snapshot ? *snapshot : empty;
}

// Publish the logical drive list as a host tag (`drives=c:,d:`), consumed by
// the web UI and by fleet group selectors. Windows-only: on other platforms
// mount points are too dynamic to make a useful identity-style tag.
void publish_drives_tag(const nscapi::core_wrapper *core) {
#ifdef WIN32
  wchar_t buffer[512];
  const DWORD length = GetLogicalDriveStringsW(511, buffer);
  if (length == 0 || length >= 512) return;
  std::string drives;
  for (const wchar_t *drive = buffer; *drive != L'\0'; drive += wcslen(drive) + 1) {
    std::string entry = utf8::cvt<std::string>(std::wstring(drive));
    while (!entry.empty() && entry.back() == '\\') entry.pop_back();
    std::transform(entry.begin(), entry.end(), entry.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (entry.empty()) continue;
    if (!drives.empty()) drives += ",";
    drives += entry;
  }
  if (!drives.empty()) core->set_tag("drives", drives);
#else
  (void)core;
#endif
}
}  // namespace

bool CheckDisk::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
  collector_.reset(new collector_thread(get_core(), get_id()));

  sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
  settings.set_alias("disk", alias);

  std::string collection_interval, trend_interval, trend_retention;
  // clang-format off
  settings.alias().add_key_to_settings()
    .add_string("disable", sh::string_key(&collector_->disable_, ""),
        "Disable automatic checks",
        "A comma separated list of checks to disable in the collector: disk_io, disk_free, trend. "
        "Please note disabling these will mean part of NSClient++ will no longer function as expected.", true)
    .add_string("collection interval", sh::string_key(&collection_interval, "10s"),
        "Collection interval",
        "How often disk I/O and disk free data is sampled. All rates (IOPS, bytes/sec) and latencies reported by check_disk_io and check_disk_health "
        "are averages over one such interval, so lowering it makes them react faster and follow short spikes more closely, at the cost of more "
        "frequent sampling. Duration, e.g. 10s.", true)
    .add_int("max collection errors", sh::int_key(&collector_->max_collection_errors, 10),
        "Maximum consecutive collection errors",
        "How many consecutive failed fetches disable a collection (disk I/O or disk free) for the rest of the process lifetime. "
        "A single failure is not treated as the source being unavailable, since the collector retries on the next interval and any success resets "
        "the count. Set to 0 to retry forever.", true)
    .add_string("trend interval", sh::string_key(&trend_interval, "5m"),
        "Trend sampling interval",
        "How often a used-space sample is kept per drive for the check_drivesize trend keywords (full_in/rate). Duration, e.g. 5m.", true)
    .add_string("trend retention", sh::string_key(&trend_retention, "7d"),
        "Trend history retention",
        "How much used-space history is kept per drive; bounds the largest useful trend-window. Duration, e.g. 7d.", true)
    ;
  // clang-format on
  settings.register_all();
  settings.notify();

  try {
    const long long interval = str::format::stox_as_time_sec<long long>(collection_interval, "s");
    // One second is the floor: it is what the Windows performance counters
    // themselves update at, so a shorter interval only costs queries. The
    // ceiling is what the collector can hold without narrowing: a value past
    // it would wrap to a negative wait, and the collector would spin.
    if (interval < 1) throw std::invalid_argument("must be at least 1 second");
    // Parenthesised so the Windows `max` macro (windows.h, included above)
    // does not eat the call.
    if (interval > (std::numeric_limits<int>::max)()) throw std::invalid_argument("is too large");
    collector_->collection_interval = static_cast<int>(interval);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR("Invalid collection interval (using the default 10s): " + std::string(e.what()));
    collector_->collection_interval = 10;
  }

  try {
    collector_->trend_interval = str::format::stox_as_time_sec<long long>(trend_interval, "s");
    collector_->trend_retention = str::format::stox_as_time_sec<long long>(trend_retention, "s");
    if (collector_->trend_interval <= 0 || collector_->trend_retention <= 0) throw std::invalid_argument("must be positive");
  } catch (const std::exception &e) {
    NSC_LOG_ERROR("Invalid trend interval/retention (using defaults 5m/7d): " + std::string(e.what()));
    collector_->trend_interval = 300;
    collector_->trend_retention = 7 * 24 * 3600;
  }
  if (collector_->max_collection_errors < 0) collector_->max_collection_errors = 0;

  if (mode == NSCAPI::normalStart) {
    collector_->start();
    publish_drives_tag(get_core());
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
    auto data = disk_health_check::join(collector_->get_disk_io(), collector_->get_disk_free(), disk_device_check::query());
    disk_health_check::check::check_disk_health(request, response, data);
  } catch (const std::exception &e) {
    nscapi::protobuf::functions::set_response_bad(*response, "Failed to get disk health data: " + std::string(e.what()));
  }
}

void CheckDisk::check_disk_write(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  try {
    check_disk_write_command::check(request, response);
  } catch (const std::exception &e) {
    nscapi::protobuf::functions::set_response_bad(*response, "Failed to run disk write test: " + std::string(e.what()));
  }
}

void CheckDisk::check_uncpath(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  try {
    uncpath_check::check::check_uncpath(request, response);
  } catch (const std::exception &e) {
    nscapi::protobuf::functions::set_response_bad(*response, "Failed to check UNC path: " + std::string(e.what()));
  }
}

void CheckDisk::check_storagepool(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  try {
    storagepool_check::check::check_storagepool(request, response);
  } catch (const std::exception &e) {
    nscapi::protobuf::functions::set_response_bad(*response, "Failed to check storage pool: " + std::string(e.what()));
  }
}

void CheckDisk::check_shadowcopy(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  try {
    shadowcopy_check::check::check_shadowcopy(request, response);
  } catch (const std::exception &e) {
    nscapi::protobuf::functions::set_response_bad(*response, "Failed to check shadow copies: " + std::string(e.what()));
  }
}

void CheckDisk::check_share(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  try {
    share_check::check::check_share(request, response);
  } catch (const std::exception &e) {
    nscapi::protobuf::functions::set_response_bad(*response, "Failed to check shares: " + std::string(e.what()));
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
#ifndef WIN32
  // Deprecated legacy command. Its argument-shim path is not supported on this
  // platform; the modern check_drivesize covers all of its functionality.
  return nscapi::protobuf::functions::set_response_bad(*response,
                                                       "checkDriveSize is a deprecated legacy command and is not supported on this platform; use "
                                                       "check_drivesize instead.");
#endif
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
  const collector_thread::trend_snapshot trends = collector_ ? collector_->get_drive_trends() : collector_thread::trend_snapshot();
  check_drive::check(request, response, deref_trends(trends));
}

void CheckDisk::check_drivesize(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  const collector_thread::trend_snapshot trends = collector_ ? collector_->get_drive_trends() : collector_thread::trend_snapshot();
  check_drive::check(request, response, deref_trends(trends));
}

void CheckDisk::checkFiles(PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
#ifndef WIN32
  // Deprecated legacy command. Its argument-shim path is not supported on this
  // platform; the modern check_files covers all of its functionality.
  return nscapi::protobuf::functions::set_response_bad(
      *response, "checkFiles is a deprecated legacy command and is not supported on this platform; use check_files instead.");
#endif
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
    ("debug", po::value<bool>(&debug)->implicit_value(true)->default_value(false), "Debug")
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

void CheckDisk::check_mount(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_mount_command::check(request, response);
}
// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "filter.hpp"

#include "duration_keyword.hpp"

#include <boost/assign.hpp>
#include <cmath>
#include <list>
#include <parsers/where/helpers.hpp>
#include <str/utils.hpp>
#include <str/xtos.hpp>

using namespace parsers::where;

namespace check_cpu_filter {
node_type calculate_load(std::shared_ptr<filter_obj> object, evaluation_context context, node_type subject) {
  helpers::read_arg_type value = helpers::read_arguments(context, subject, "%");
  const double number = value.get<1>();
  const std::string unit = value.get<2>();

  if (unit != "%") context->error("Invalid unit: " + unit);
  return factory::create_int(llround(number));
}

filter_obj_handler::filter_obj_handler() {
  static constexpr value_type type_custom_pct = type_custom_int_1;

  registry_.add_string_var("time", &filter_obj::get_time, "The time frame to check")
      .add_string_var("core", &filter_obj::get_core_s, &filter_obj::get_core_i, "The core to check (total or core ##)")
      .add_string_var("core_id", &filter_obj::get_core_id, &filter_obj::get_core_i, "The core to check (total or core_##)");
  registry_.add_int_var("load", type_custom_pct, &filter_obj::get_total, "deprecated (use usage instead)")
      .add_int_perf("%")
      .add_int_var("usage", type_custom_pct, &filter_obj::get_total, "The current load used by user and system")
      .add_int_perf("%", "", "_total")
      .add_int_var("total", type_custom_pct, &filter_obj::get_total,
                   "Deprecated alias for usage (the name clashes with the generic total summary keyword).")
      .add_int_perf("%", "", "_total")
      .add_int_var("user", type_custom_pct, &filter_obj::get_user, "The current load used by user applications")
      .add_int_perf("%", "", "_user")
      .add_int_var("idle", &filter_obj::get_idle, "The current idle load for a given core")
      .add_int_perf("%", "", "_idle")
      .add_int_var("system", &filter_obj::get_kernel, "The current load used by the system (kernel)")
      .add_int_perf("%", "", "_system")
      // kernel is a deprecated alias of system and reports the same value; it is
      // intentionally left without perf so we do not emit a duplicate perf
      // column for the same metric (the value is already graphed as system).
      .add_int_var("kernel", &filter_obj::get_kernel, "deprecated (use system instead)");

  registry_.add_converter(type_custom_pct, &calculate_load);
}
}  // namespace check_cpu_filter

namespace check_page_filter {
node_type calculate_free(std::shared_ptr<filter_obj> object, evaluation_context context, node_type subject) {
  helpers::read_arg_type value = helpers::read_arguments(context, subject, "%");
  double number = value.get<1>();
  const std::string unit = value.get<2>();

  if (unit == "%") {
    number = (static_cast<double>(object->get_total()) * number) / 100.0;
  } else {
    number = str::format::decode_byte_units(number, unit);
  }
  return factory::create_int(llround(number));
}

long long get_zero() { return 0; }

filter_obj_handler::filter_obj_handler() {
  static constexpr value_type type_custom_used = type_custom_int_1;
  static constexpr value_type type_custom_free = type_custom_int_2;

  registry_.add_string_var("name", &filter_obj::get_name, "The name of the page file (location)");
  registry_.add_int_legacy()
      .add_int("size", &filter_obj::get_total, "Total size of pagefile")
      .add_int("free", type_custom_free, &filter_obj::get_free, "Free memory in bytes (g,m,k,b) or percentages %")
      .add_scaled_byte([](auto obj, auto context) { return get_zero(); }, [](auto obj, auto context) { return obj->get_total(); })
      .add_percentage([](auto obj, auto context) { return obj->get_total(); }, "", " %")
      .add_int("used", type_custom_used, &filter_obj::get_used, "Used memory in bytes (g,m,k,b) or percentages %")
      .add_scaled_byte([](auto obj, auto context) { return get_zero(); }, [](auto obj, auto context) { return obj->get_total(); })
      .add_percentage([](auto obj, auto context) { return obj->get_total(); }, "", " %")
      .add_int("free_pct", &filter_obj::get_free_pct, "% free memory")
      .add_int("used_pct", &filter_obj::get_used_pct, "% used memory")
      // Peak commit charge for this pagefile since boot (Win32_PageFileUsage.PeakUsage
      // equivalent, sourced from SystemPageFileInformation). Lets admins alert on the
      // high-water mark, not just the instantaneous usage.
      .add_int("peak_used", &filter_obj::get_peak, "Peak used memory in bytes (g,m,k,b) since boot")
      .add_scaled_byte([](auto obj, auto context) { return get_zero(); }, [](auto obj, auto context) { return obj->get_total(); })
      .add_int("peak_used_pct", &filter_obj::get_peak_used_pct, "% peak used memory since boot");
  registry_.add_human_string_context("size", &filter_obj::get_total_human, "")
      .add_human_string_context("free", &filter_obj::get_free_human, "")
      .add_human_string_context("used", &filter_obj::get_used_human, "")
      // Issue #595: render percentages with two decimals via human-string
      .add_human_string_context("used_pct", &filter_obj::get_used_pct_human, "")
      .add_human_string_context("free_pct", &filter_obj::get_free_pct_human, "")
      .add_human_string_context("peak_used", &filter_obj::get_peak_human, "")
      .add_human_string_context("peak_used_pct", &filter_obj::get_peak_used_pct_human, "");

  registry_.add_converter(type_custom_free, &calculate_free).add_converter(type_custom_used, &calculate_free);
}
}  // namespace check_page_filter

namespace check_uptime_filter {
static const value_type type_custom_uptime = type_custom_int_1;
filter_obj_handler::filter_obj_handler() {
  registry_.add_int_var("boot", type_date, &filter_obj::get_boot, "System boot time")
      .add_int_var("uptime", type_custom_uptime, &filter_obj::get_uptime, "Time since last boot")
      .add_int_perf("s", "", "");
  // The shared duration converter (issues #452 + #589): "2d"/"30m" become
  // seconds, plain integers pass through untouched.
  registry_.add_converter(type_custom_uptime, &duration_keyword::parse_duration<std::shared_ptr<filter_obj> >);
  registry_.add_human_string("boot", &filter_obj::get_boot_s, "The system boot time")
      .add_human_string("uptime", &filter_obj::get_uptime_s, "Time since last boot (granularity controlled by --max-unit)")
      .add_human_string("tz", &filter_obj::get_tz, "The timezone label used to render boot time");
}
}  // namespace check_uptime_filter

namespace os_version_filter {
filter_obj_handler::filter_obj_handler() {
  registry_.add_int_var("major", &filter_obj::get_major, "Major version number (perfdata)")
      .add_int_perf("", "", "_major")
      .add_int_var("version", &filter_obj::get_version_i, &filter_obj::get_version_s,
                   "System version: numeric for thresholds (major*10+minor, e.g. 'version <= 50'), rendered as the friendly product name (e.g. 'Windows 11 "
                   "23H2')")
      .add_int_perf("")
      .add_int_var("minor", &filter_obj::get_minor, "Minor version number (perfdata)")
      .add_int_perf("", "", "_minor")
      .add_int_var("build", &filter_obj::get_build, "Build version number (perfdata)")
      .add_int_perf("", "", "_build")
      .add_int_var("ubr", &filter_obj::get_ubr,
                   "Update Build Revision — the patch level within a build (the .3803 in 10.0.19045.3803), read from the registry; 0 when unavailable (e.g. "
                   "pre-Windows 10)");
  registry_.add_string_var(
      "suite", &filter_obj::get_suite_string,
      "Which suites are installed on the machine (Microsoft BackOffice, Web Edition, Compute Cluster Edition, Datacenter Edition, Enterprise "
      "Edition, Embedded, Home Edition, Remote Desktop Support, Small Business Server, Storage Server, Terminal Services, Home Server)");
  registry_
      .add_string_var("arch", &filter_obj::get_arch,
                      "Native processor architecture: x64, x86, arm64, arm, ia64 or unknown (via GetNativeSystemInfo, so a 32-bit agent under WOW64 still "
                      "reports the true hardware architecture)")
      .add_string_var("kernel_version", &filter_obj::get_kernel_version,
                      "NT kernel version as major.minor.build.ubr (on Windows the kernel version tracks the OS version)");
  // Inventory-only fields from Win32_BIOS (never alert; empty if WMI is unavailable).
  registry_.add_string_var("serial", &filter_obj::get_serial, "BIOS/system serial number (Win32_BIOS.SerialNumber); inventory only, empty when WMI is unavailable")
      .add_string_var("bios_version", &filter_obj::get_bios_version, "BIOS version (Win32_BIOS.SMBIOSBIOSVersion); inventory only, empty when WMI is unavailable")
      .add_string_var("manufacturer", &filter_obj::get_manufacturer,
                      "BIOS manufacturer / vendor (Win32_BIOS.Manufacturer); inventory only, empty when WMI is unavailable");
}

std::string arch_from_native(unsigned short processor_architecture) {
  switch (processor_architecture) {
    case PROCESSOR_ARCHITECTURE_AMD64:
      return "x64";
    case PROCESSOR_ARCHITECTURE_ARM64:
      return "arm64";
    case PROCESSOR_ARCHITECTURE_ARM:
      return "arm";
    case PROCESSOR_ARCHITECTURE_IA64:
      return "ia64";
    case PROCESSOR_ARCHITECTURE_INTEL:
      return "x86";
    default:
      return "unknown";
  }
}

std::string format_kernel_version(long long major, long long minor, long long build, long long ubr) {
  return str::xtos(major) + "." + str::xtos(minor) + "." + str::xtos(build) + "." + str::xtos(ubr);
}
}  // namespace os_version_filter
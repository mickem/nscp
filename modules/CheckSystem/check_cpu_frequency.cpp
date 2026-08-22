// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_cpu_frequency.hpp"

#include <boost/thread/locks.hpp>
#include <nscapi/nscapi_metrics_helper.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <str/format.hpp>

namespace cpu_frequency_check {

// Win32_Processor provides per-socket CPU info including current and max clock speed.
std::string helper::query =
    "select DeviceId, SocketDesignation, Name, CurrentClockSpeed, MaxClockSpeed, NumberOfCores, NumberOfLogicalProcessors, LoadPercentage,"
    " L2CacheSize, L3CacheSize, Architecture"
    " from Win32_Processor";
std::string helper::ns = "root\\CIMV2";

std::string architecture_to_string(const long long architecture) {
  // Win32_Processor.Architecture values.
  switch (architecture) {
    case 0:
      return "x86";
    case 1:
      return "MIPS";
    case 2:
      return "Alpha";
    case 3:
      return "PowerPC";
    case 5:
      return "ARM";
    case 6:
      return "ia64";
    case 9:
      return "x64";
    case 12:
      return "ARM64";
    default:
      return "unknown (" + std::to_string(architecture) + ")";
  }
}

namespace {
// The inventory columns are absent or NULL on some platforms (VMs, older
// Windows); read them best-effort instead of failing the whole row.
long long get_int_or_zero(const wmi_impl::row &r, const char *col) {
  try {
    return r.get_int(col);
  } catch (...) {
    return 0;
  }
}
}  // namespace

void cpu_frequency::read_wmi(const wmi_impl::row &r) {
  socket_id = r.get_string("DeviceId");
  socket = r.get_string("SocketDesignation");
  name = r.get_string("Name");
  current_mhz = r.get_int("CurrentClockSpeed");
  max_mhz = r.get_int("MaxClockSpeed");
  number_of_cores = r.get_int("NumberOfCores");
  number_of_logical_processors = r.get_int("NumberOfLogicalProcessors");
  load_pct = r.get_int("LoadPercentage");
  // L2/L3CacheSize are reported in KB.
  l2_cache = get_int_or_zero(r, "L2CacheSize") * 1024;
  l3_cache = get_int_or_zero(r, "L3CacheSize") * 1024;
  try {
    architecture = architecture_to_string(r.get_int("Architecture"));
  } catch (...) {
    // Cannot default to 0 here: 0 is a valid value (x86).
    architecture = "unknown";
  }
}

std::string cpu_frequency::get_l2_cache_human(parsers::where::evaluation_context context) const {
  return str::format::format_byte_units(l2_cache, context->get_number_format());
}
std::string cpu_frequency::get_l3_cache_human(parsers::where::evaluation_context context) const {
  return str::format::format_byte_units(l3_cache, context->get_number_format());
}

void cpu_frequency::build_metrics(PB::Metrics::MetricsBundle *section) const {
  using namespace nscapi::metrics;
  add_metric(section, name + ".current_mhz", current_mhz);
  add_metric(section, name + ".max_mhz", max_mhz);
  add_metric(section, name + ".frequency_pct", get_frequency_pct());
  add_metric(section, name + ".cores", number_of_cores);
  add_metric(section, name + ".logical_processors", number_of_logical_processors);
  add_metric(section, name + ".load_pct", load_pct);
  add_metric(section, name + ".l2_cache", l2_cache);
  add_metric(section, name + ".l3_cache", l3_cache);
}

cpus_type cpu_frequency_data::query_wmi() {
  wmi_impl::query wmi_q(helper::query, helper::ns, "", "");
  wmi_impl::row_enumerator row = wmi_q.execute();
  cpus_type cpus;
  while (row.has_next()) {
    const wmi_impl::row r = row.get_next();
    cpu_frequency c;
    c.read_wmi(r);
    cpus.push_back(c);
  }
  return cpus;
}

void cpu_frequency_data::fetch() {
  if (!fetch_cpu_frequency_) return;

  try {
    const cpus_type tmp = query_wmi();
    const boost::unique_lock<boost::shared_mutex> write_lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
    if (!write_lock.owns_lock()) throw nsclient::nsclient_exception("Failed to get mutex for writing CPU frequency data");
    cpus_ = tmp;
  } catch (const wmi_impl::wmi_exception &e) {
    if (e.get_code() == WBEM_E_INVALID_QUERY || e.get_code() == WBEM_E_NOT_FOUND) {
      fetch_cpu_frequency_ = false;
      throw nsclient::nsclient_exception("Failed to fetch CPU frequency metrics (WMI class not available), disabling...");
    }
    throw nsclient::nsclient_exception("Failed to fetch CPU frequency metrics: " + e.reason());
  }
}

cpus_type cpu_frequency_data::get() {
  const boost::shared_lock<boost::shared_mutex> read_lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!read_lock.owns_lock()) throw nsclient::nsclient_exception("Failed to get mutex for reading CPU frequency data");
  return cpus_;
}

namespace check {

typedef cpu_frequency filter_obj;

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("name", &filter_obj::get_name, "CPU name / model string")
      .add_string_var("socket_id", &filter_obj::get_socket_id, "Socket device id (e.g. CPU0), for per-socket filtering")
      .add_string_var("socket", &filter_obj::get_socket, "Socket designation (e.g. \"CPU 1\"), for per-socket filtering");

  registry_.add_int_var("current_mhz", &filter_obj::get_current_mhz, "Current clock speed in MHz (perfdata)")
      .add_int_perf("MHz")
      .add_int_var("max_mhz", &filter_obj::get_max_mhz, "Maximum clock speed in MHz (perfdata)")
      .add_int_perf("MHz", "", "_max_mhz")
      .add_int_var("frequency_pct", &filter_obj::get_frequency_pct, "Current frequency as percentage of maximum (perfdata)")
      .add_int_perf("%", "", "_frequency_pct")
      .add_int_var("load_pct", &filter_obj::get_load_pct, "Per-socket CPU load as reported by Win32_Processor.LoadPercentage (perfdata)")
      .add_int_perf("%", "", "_load_pct")
      .add_int_var("cores", &filter_obj::get_number_of_cores, "Number of physical cores")
      .add_int_var("logical_processors", &filter_obj::get_number_of_logical_processors, "Number of logical processors (threads)")
      .add_int_var("l2_cache", parsers::where::type_size, &filter_obj::get_l2_cache,
                   "L2 cache size (size units work, e.g. 'l2_cache < 1M'); renders human-readable; 0 when not reported")
      .add_int_var("l3_cache", parsers::where::type_size, &filter_obj::get_l3_cache, "L3 cache size; 0 when not reported (common on VMs)");

  registry_.add_string_var("architecture", &filter_obj::get_architecture, "Processor architecture (x86, x64, ARM64, ...)");

  // Render the cache sizes human-readable; expressions keep comparing bytes.
  registry_.add_human_string_context("l2_cache", &filter_obj::get_l2_cache_human, "L2 cache as a human-readable size")
      .add_human_string_context("l3_cache", &filter_obj::get_l3_cache_human, "L3 cache as a human-readable size");
}

void check_cpu_frequency(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                         const cpus_type &data) {
  modern_filter::data_container mdata;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, mdata);

  filter_type filter;
  // No default warn/crit thresholds (parity with the Linux implementation):
  // modern CPUs legitimately clock far below their maximum at idle, so a
  // frequency_pct default would warn on every idle machine.
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${list}", "${name}: ${current_mhz}/${max_mhz} MHz (${frequency_pct}%)", "${name}", "",
                           "%(status): All CPU frequencies seem ok.");

  if (!filter_helper.parse_options()) return;

  if (!filter_helper.build_filter(filter)) return;

  // Same contract as the Linux implementation: no data (or a collector cache
  // that has not been populated yet) is UNKNOWN, not an empty-state trip.
  if (data.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, "No CPU frequency data available");
  }

  for (const cpu_frequency &c : data) {
    const std::shared_ptr<filter_obj> record(new filter_obj(c));
    filter.match(record);
  }
  filter_helper.post_process(filter);
}

}  // namespace check

}  // namespace cpu_frequency_check

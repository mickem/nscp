// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_printqueue.hpp"

#include <Windows.h>
#include <comdef.h>

#include <boost/algorithm/string.hpp>
#include <ctime>
#include <map>
#include <memory>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <str/format.hpp>
#include <win/com_helpers.hpp>
#include <win/wmi/wmi_query.hpp>

#include <check/duration_keyword.hpp>

namespace printqueue_check {

namespace {
// The Windows spooler flags a job error with JOB_STATUS_ERROR in StatusMask.
constexpr long long kJobStatusError = 0x00000002;

// Optional Win32_Printer properties: absent or NULL on many queues, so read them
// best-effort rather than letting one missing value cost us the whole row.
std::string read_string(const wmi_impl::row &r, const char *column) {
  try {
    const std::string value = r.get_string(column);
    return value == "<NULL>" ? std::string() : value;
  } catch (...) {
    return {};
  }
}
bool read_bool(const wmi_impl::row &r, const char *column) {
  try {
    return r.get_int(column) != 0;  // WMI VT_BOOL arrives as -1/0 through get_int
  } catch (...) {
    return false;
  }
}
}  // namespace

// CIM_DATETIME parsing lives in str::format (shared with other WMI checks); this
// thin wrapper keeps the check's testable entry point.
long long parse_cim_datetime(const std::string &s) { return str::format::parse_cim_datetime(s); }

std::string job_printer_name(const std::string &job_name) {
  // Win32_PrintJob.Name is "<PrinterName>, <JobId>"; the job id is numeric and
  // follows the last ", ", so strip from there back.
  const std::size_t sep = job_name.rfind(", ");
  if (sep == std::string::npos) return job_name;
  return job_name.substr(0, sep);
}

printers_type build_printers(const std::vector<printer_info> &printers, const std::vector<raw_job> &jobs, long long now_epoch) {
  // Index printers by lower-cased name for the join (printer names are
  // case-insensitive on Windows).
  std::map<std::string, printer_info> by_name;
  std::vector<std::string> order;
  for (const printer_info &p : printers) {
    printer_info row = p;
    row.jobs = 0;
    row.error_jobs = 0;
    row.oldest_epoch = 0;
    row.now_epoch = now_epoch;
    const std::string key = boost::to_lower_copy(p.name);
    if (by_name.find(key) == by_name.end()) order.push_back(key);
    by_name[key] = row;
  }

  for (const raw_job &j : jobs) {
    const auto it = by_name.find(boost::to_lower_copy(j.printer));
    if (it == by_name.end()) continue;  // job for a printer we did not enumerate
    printer_info &p = it->second;
    p.jobs++;
    if (j.error) p.error_jobs++;
    if (j.submitted_epoch > 0 && (p.oldest_epoch == 0 || j.submitted_epoch < p.oldest_epoch)) p.oldest_epoch = j.submitted_epoch;
  }

  printers_type out;
  for (const std::string &key : order) out.push_back(by_name[key]);
  return out;
}

void gather(std::vector<printer_info> &printers, std::vector<raw_job> &jobs) {
  // Scoped COM init: balanced on every exit path, including the exceptions the
  // queries below may throw.
  const com_helper::mta_scope com;
  {
    {
      // All named columns are fixed core-schema Win32_Printer properties, so a
      // projection is safe; the per-column reads below still tolerate NULL
      // values (Location, ShareName and ServerName are NULL on most local
      // queues). Naming the columns matters here: SELECT * would materialize
      // ~85 properties per printer, including the array-valued capability
      // lists, for the 12 scalars this check reads.
      wmi_impl::query q(
          "select Name, PrinterStatus, DetectedErrorState, WorkOffline, DriverName, PortName, Location, ShareName, ServerName, Default, Shared, Network"
          " from Win32_Printer",
          "root\\CIMV2", "", "");
      wmi_impl::row_enumerator rows = q.execute();
      while (rows.has_next()) {
        const wmi_impl::row r = rows.get_next();
        printer_info p;
        p.name = r.get_string("Name");
        try {
          p.printer_status = r.get_int("PrinterStatus");
        } catch (...) {
        }
        try {
          p.error_state = r.get_int("DetectedErrorState");
        } catch (...) {
        }
        try {
          p.work_offline = r.get_int("WorkOffline") != 0;
        } catch (...) {
        }
        p.driver = read_string(r, "DriverName");
        p.port = read_string(r, "PortName");
        p.location = read_string(r, "Location");
        p.share = read_string(r, "ShareName");
        p.server = read_string(r, "ServerName");
        p.is_default = read_bool(r, "Default");
        p.is_shared = read_bool(r, "Shared");
        p.is_network = read_bool(r, "Network");
        if (!p.name.empty()) printers.push_back(p);
      }
    }
    {
      wmi_impl::query q("select Name, TimeSubmitted, StatusMask from Win32_PrintJob", "root\\CIMV2", "", "");
      wmi_impl::row_enumerator rows = q.execute();
      while (rows.has_next()) {
        const wmi_impl::row r = rows.get_next();
        raw_job j;
        j.printer = job_printer_name(r.get_string("Name"));
        j.submitted_epoch = parse_cim_datetime(r.get_string("TimeSubmitted"));
        try {
          j.error = (r.get_int("StatusMask") & kJobStatusError) != 0;
        } catch (...) {
        }
        jobs.push_back(j);
      }
    }
  }
}

namespace check {

typedef printer_info filter_obj;

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

// oldest_job_age is seconds; the converter is what makes the documented
// `oldest_job_age > 30m` mean thirty minutes rather than the number 30.
static const parsers::where::value_type type_custom_age = parsers::where::type_custom_int_1;

filter_obj_handler::filter_obj_handler() {
  using parsers::where::type_bool;
  using parsers::where::type_int;
  registry_.add_string_var("printer", &filter_obj::get_printer, "Printer / queue name")
      .add_string_var("printer_status", &filter_obj::get_status, "Printer status: idle, printing, offline, stopped_printing, warmup, ...")
      .add_string_var("status", &filter_obj::get_status, "Deprecated alias for printer_status (the name clashes with the generic status summary keyword).")
      .add_string_var("error_state", &filter_obj::get_error_state, "Detected error state: no_error, no_paper, jammed, door_open, ...")
      .add_string_var("driver", &filter_obj::get_driver, "Print driver the queue uses")
      .add_string_var("port", &filter_obj::get_port, "Port the queue prints through (IP_x.x.x.x, USB001, PORTPROMPT:, ...)")
      .add_string_var("location", &filter_obj::get_location, "Location as configured on the queue (empty when unset)")
      .add_string_var("share", &filter_obj::get_share, "Share name (empty when the queue is not shared)")
      .add_string_var("server", &filter_obj::get_server, "Print server hosting the queue (empty for a local queue)");
  // Distinct perf suffixes so co-referenced metrics each get their own series.
  registry_.add_int_var("jobs", &filter_obj::get_jobs, "Number of queued print jobs")
      .add_int_perf("", "", "_jobs")
      .add_int_var("error_jobs", &filter_obj::get_error_jobs, "Number of queued jobs in an error state")
      .add_int_perf("", "", "_error_jobs")
      .add_int_var("oldest_job_age", type_custom_age, &filter_obj::get_oldest_job_age,
                   "Seconds since the oldest queued job (-1 if the queue is empty); threshold with durations, e.g. oldest_job_age > 30m")
      .add_int_perf("s", "", "_oldest_job_age")
      .add_int_var("offline", type_bool, &filter_obj::get_offline, "1 if the printer is offline")
      .no_perf()
      .add_int_var("error", type_bool, &filter_obj::get_error, "1 if the printer is in a real error state (paper/toner/door/jam/service)")
      .no_perf()
      .add_int_var("default", type_bool, &filter_obj::get_default, "1 if this is the default printer")
      .no_perf()
      .add_int_var("shared", type_bool, &filter_obj::get_shared, "1 if the queue is shared")
      .no_perf()
      .add_int_var("network", type_bool, &filter_obj::get_network, "1 if the queue is a network (rather than local) printer")
      .no_perf();
  registry_.add_converter(type_custom_age, &duration_keyword::parse_duration<std::shared_ptr<filter_obj> >);
}

void check_printqueue_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                           const std::vector<printer_info> &printers, const std::vector<raw_job> &jobs, const long long now_epoch) {
  modern_filter::data_container mdata;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, mdata);

  filter_type filter;
  // Default: WARNING on a backed-up queue, CRITICAL on a real printer error.
  // Offline printers are common (virtual/disconnected), so they are NOT alerted
  // by default — use the `offline` keyword to opt in. empty_state=ok: a host
  // with no printers is not a problem.
  filter_helper.add_options("jobs > 10", "error = 1", "", filter.get_filter_syntax(), "ok");
  filter_helper.add_syntax("${status}: ${list}", "${printer}: ${printer_status}, ${jobs} job(s)", "${printer}", "%(status): No printers found",
                           "%(status): All %(count) printer(s) ok.");

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  for (const printer_info &p : build_printers(printers, jobs, now_epoch)) {
    std::shared_ptr<filter_obj> record(new filter_obj(p));
    filter.match(record);
  }

  filter_helper.post_process(filter);
}

void check_printqueue(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  std::vector<printer_info> printers;
  std::vector<raw_job> jobs;
  gather(printers, jobs);
  check_printqueue_from(request, response, printers, jobs, static_cast<long long>(std::time(nullptr)));
}

}  // namespace check

}  // namespace printqueue_check

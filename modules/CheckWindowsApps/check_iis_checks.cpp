// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_iis_checks.hpp"

#include <boost/algorithm/string/trim.hpp>
#include <boost/program_options.hpp>
#include <map>
#include <memory>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>
#include <vector>
#include <win/com_helpers.hpp>
#include <win/pdh/pdh_interface.hpp>
#include <win/pdh/pdh_object_gather.hpp>
#include <win/wmi/wmi_query.hpp>

#include "check_iis_internal.hpp"

namespace po = boost::program_options;

namespace check_iis {

using namespace check_iis_internal;

namespace {

// Fetch Name -> auto-start from root\WebAdministration. The namespace only
// exists when the "IIS Management Scripts and Tools" feature is installed, so
// a failure is expected on many hosts and simply disables the enrichment.
bool fetch_auto_start(const std::string &wmi_class, const std::string &property, std::map<std::string, bool> &out) {
  try {
    wmi_impl::query wmi_query("SELECT Name, " + property + " FROM " + wmi_class, "root\\WebAdministration", "", "");
    wmi_impl::row_enumerator rows = wmi_query.execute();
    while (rows.has_next()) {
      const wmi_impl::row &row = rows.get_next();
      // VARIANT_TRUE converts to -1, so compare against 0.
      out[row.get_string("Name")] = row.get_int(property) != 0;
    }
    return true;
  } catch (const wmi_impl::wmi_exception &e) {
    NSC_DEBUG_MSG_STD("IIS WMI provider not available (skipping " + wmi_class + " enrichment): " + e.reason());
    return false;
  }
}

std::string iis_counters_error(const std::string &object, const PDH::pdh_exception &e) {
  // PDH messages come from FormatMessage and end in \r\n; trim so the check
  // output stays a single line.
  std::string reason = e.reason();
  boost::algorithm::trim(reason);
  return "IIS performance counters (" + object + ") not available - is the Web Server (IIS) role installed? (" + reason + ")";
}

}  // namespace

// ---------------------------------------------------------------------------
// check_iis_app_pools
// ---------------------------------------------------------------------------

namespace app_pool_filter {

struct filter_obj {
  pool_record rec;
  explicit filter_obj(pool_record rec) : rec(std::move(rec)) {}

  std::string show() const { return rec.name + " (" + pool_state_name(rec.state) + ")"; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler() {
    registry_.add_string_var("pool", [](auto obj) { return obj->rec.name; }, "Name of the application pool");
    registry_.add_string_var("state", [](auto obj) { return pool_state_name(obj->rec.state); },
                             "Pool state: running, disabled, disabling, shutdown_pending, delete_pending, initialized, uninitialized or unknown");
    registry_.add_int_var("state_id", parsers::where::type_int, [](auto obj) { return obj->rec.state; }, "Raw APP_POOL_WAS state value (3 = running)");
    registry_.add_int_var("uptime", parsers::where::type_int, [](auto obj) { return obj->rec.uptime; }, "Seconds since the pool last started")
        .add_int_perf("s", "", "_uptime");
    registry_.add_int_var("recycles", parsers::where::type_int, [](auto obj) { return obj->rec.recycles; }, "Pool recycles since WAS started")
        .add_int_perf("c", "", "_recycles");
    registry_.add_int_var("auto_start", parsers::where::type_int, [](auto obj) { return obj->rec.auto_start; },
                          "1 when the pool is set to start automatically, 0 when not, -1 when the IIS WMI provider is unavailable");
  }
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace app_pool_filter

void check_iis_app_pools(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  using app_pool_filter::filter;
  using app_pool_filter::filter_obj;

  modern_filter::data_container data;
  modern_filter::cli_helper<filter> filter_helper(request, response, data);

  filter f;
  // auto_start != 0 also matches -1 (WMI unavailable): without configuration
  // state every non-running pool alerts, with it manually-stopped pools stay
  // quiet.
  filter_helper.add_options("", "state != 'running' and auto_start != 0", "", f.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${list}", "${pool}: ${state}, uptime ${uptime}s, ${recycles} recycles", "${pool}", "No application pools found", "");
  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(f)) return;
  f.add_manual_perf("uptime");
  f.add_manual_perf("recycles");

  // COM for the WMI enrichment; harmless when COM is already up.
  const com_helper::mta_scope com;
  instance_values pools;
  try {
    pools = PDH::gather_object_instances("APP_POOL_WAS",
                                         {"Current Application Pool State", "Current Application Pool Uptime", "Total Application Pool Recycles"}, false);
  } catch (const PDH::pdh_exception &e) {
    return nscapi::protobuf::functions::set_response_bad(*response, iis_counters_error("APP_POOL_WAS", e));
  }
  std::map<std::string, bool> auto_start;
  fetch_auto_start("ApplicationPool", "AutoStart", auto_start);

  for (const pool_record &rec : merge_pools(pools, auto_start)) f.match(std::make_shared<filter_obj>(rec));

  filter_helper.post_process(f);
}

// ---------------------------------------------------------------------------
// check_iis_sites
// ---------------------------------------------------------------------------

namespace site_filter {

struct filter_obj {
  site_record rec;
  explicit filter_obj(site_record rec) : rec(std::move(rec)) {}

  // A stopped site has no Web Service counter instance (the counters only
  // exist for started sites); a zero uptime sample alone must NOT mean
  // stopped — a site recycled less than a second ago, or a transient PDH
  // formatting failure (reported as 0), would page for a healthy site.
  std::string state() const { return rec.in_pdh ? "running" : "stopped"; }
  std::string show() const { return rec.name + " (" + state() + ")"; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler() {
    registry_.add_string_var("site", [](auto obj) { return obj->rec.name; }, "Name of the web site");
    registry_.add_string_var("state", [](auto obj) { return obj->state(); }, "running or stopped (stopped = the site has no Web Service counter instance)");
    registry_.add_int_var("connections", parsers::where::type_int, [](auto obj) { return obj->rec.connections; }, "Current connections to the site")
        .add_int_perf("", "", "_connections");
    registry_.add_int_var("uptime", parsers::where::type_int, [](auto obj) { return obj->rec.uptime; }, "Seconds the site has been up (0 when stopped)")
        .add_int_perf("s", "", "_uptime");
    registry_.add_float("requests_per_sec", [](auto obj) { return obj->rec.requests_per_sec; },
                        "Requests per second (needs averages=true, otherwise 0)")
        .add_float_perf("", "", "_requests_per_sec");
    registry_.add_float("bytes_per_sec", [](auto obj) { return obj->rec.bytes_per_sec; }, "Bytes sent+received per second (needs averages=true, otherwise 0)")
        .add_float_perf("B", "", "_bytes_per_sec");
    registry_.add_int_var("auto_start", parsers::where::type_int, [](auto obj) { return obj->rec.auto_start; },
                          "1 when the site is set to start automatically, 0 when not, -1 when the IIS WMI provider is unavailable");
  }
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace site_filter

void check_iis_sites(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  using site_filter::filter;
  using site_filter::filter_obj;

  modern_filter::data_container data;
  modern_filter::cli_helper<filter> filter_helper(request, response, data);
  bool averages = false;

  filter f;
  filter_helper.add_options("", "state = 'stopped' and auto_start != 0", "", f.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${list}", "${site}: ${state}, ${connections} connections, uptime ${uptime}s", "${site}", "No web sites found", "");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("averages", po::value<bool>(&averages)->implicit_value(true)->default_value(false),
        "Collect a second sample after one second so the rate keywords (requests_per_sec, bytes_per_sec) carry real values.")
    ;
  // clang-format on
  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(f)) return;
  f.add_manual_perf("connections");

  // COM for the WMI enrichment; harmless when COM is already up.
  const com_helper::mta_scope com;
  instance_values sites;
  try {
    sites = PDH::gather_object_instances("Web Service", {"Current Connections", "Service Uptime", "Total Method Requests/sec", "Bytes Total/sec"}, averages);
  } catch (const PDH::pdh_exception &e) {
    return nscapi::protobuf::functions::set_response_bad(*response, iis_counters_error("Web Service", e));
  }
  std::map<std::string, bool> auto_start;
  fetch_auto_start("Site", "ServerAutoStart", auto_start);

  for (const site_record &rec : merge_sites(sites, auto_start)) f.match(std::make_shared<filter_obj>(rec));

  filter_helper.post_process(f);
}

// ---------------------------------------------------------------------------
// check_iis_worker_processes
// ---------------------------------------------------------------------------

namespace worker_filter {

struct filter_obj {
  std::string instance;
  long long pid = 0;
  std::string pool;
  long long active_requests = 0;
  long long total_requests = 0;

  std::string show() const { return pool + " (pid " + std::to_string(pid) + ", " + std::to_string(active_requests) + " active)"; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler() {
    registry_.add_string_var("pool", [](auto obj) { return obj->pool; }, "Application pool the worker serves");
    registry_.add_string_var("instance", [](auto obj) { return obj->instance; }, "Raw counter instance name (<pid>_<pool>)");
    registry_.add_int_var("pid", parsers::where::type_int, [](auto obj) { return obj->pid; }, "Process id of the w3wp worker");
    registry_.add_int_var("active_requests", parsers::where::type_int, [](auto obj) { return obj->active_requests; }, "Requests currently executing in the worker")
        .add_int_perf("", "", "_active_requests");
    registry_.add_int_var("total_requests", parsers::where::type_int, [](auto obj) { return obj->total_requests; }, "HTTP requests served since the worker started")
        .add_int_perf("c", "", "_total_requests");
  }
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace worker_filter

void check_iis_worker_processes(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  using worker_filter::filter;
  using worker_filter::filter_obj;

  modern_filter::data_container data;
  modern_filter::cli_helper<filter> filter_helper(request, response, data);

  filter f;
  // No workers is a normal state (idle pools spin their workers down), so the
  // empty set is OK by default rather than unknown.
  filter_helper.add_options("", "", "", f.get_filter_syntax(), "ok");
  filter_helper.add_syntax("${status}: ${list}", "${pool} (pid ${pid}): ${active_requests} active requests", "${pool}_${pid}",
                           "No IIS worker processes running", "");
  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(f)) return;
  f.add_manual_perf("active_requests");

  instance_values workers;
  try {
    workers = PDH::gather_object_instances("W3SVC_W3WP", {"Active Requests", "Total HTTP Requests Served"}, false);
  } catch (const PDH::pdh_exception &e) {
    return nscapi::protobuf::functions::set_response_bad(*response, iis_counters_error("W3SVC_W3WP", e));
  }

  for (const auto &entry : workers) {
    auto obj = std::make_shared<filter_obj>();
    obj->instance = entry.first;
    parse_worker_instance(entry.first, obj->pid, obj->pool);
    obj->active_requests = static_cast<long long>(PDH::value_of(entry.second, "Active Requests"));
    obj->total_requests = static_cast<long long>(PDH::value_of(entry.second, "Total HTTP Requests Served"));
    f.match(obj);
  }

  filter_helper.post_process(f);
}

// ---------------------------------------------------------------------------
// check_iis_request_queues
// ---------------------------------------------------------------------------

namespace queue_filter {

struct filter_obj {
  std::string name;
  long long queue_length = 0;
  long long rejected = 0;
  long long max_age = 0;

  std::string show() const { return name + " (" + std::to_string(queue_length) + " queued)"; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler() {
    registry_.add_string_var("queue", [](auto obj) { return obj->name; }, "Name of the HTTP.sys request queue (usually the application pool)");
    registry_.add_int_var("queue_length", parsers::where::type_int, [](auto obj) { return obj->queue_length; }, "Requests currently waiting in the queue")
        .add_int_perf("", "", "_queue_length");
    registry_.add_int_var("rejected", parsers::where::type_int, [](auto obj) { return obj->rejected; }, "Requests rejected from the queue since it was created")
        .add_int_perf("c", "", "_rejected");
    registry_.add_int_var("max_age", parsers::where::type_int, [](auto obj) { return obj->max_age; }, "Age of the oldest request in the queue")
        .add_int_perf("", "", "_max_age");
  }
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace queue_filter

void check_iis_request_queues(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  using queue_filter::filter;
  using queue_filter::filter_obj;

  modern_filter::data_container data;
  modern_filter::cli_helper<filter> filter_helper(request, response, data);

  filter f;
  // 1000 is HTTP.sys' default per-queue limit; warn as it fills, go critical
  // when it is effectively full (rejections start).
  filter_helper.add_options("queue_length > 800", "queue_length > 1000", "", f.get_filter_syntax(), "ok");
  filter_helper.add_syntax("${status}: ${list}", "${queue}: ${queue_length} queued, ${rejected} rejected", "${queue}", "No HTTP.sys request queues found", "");
  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(f)) return;
  f.add_manual_perf("rejected");

  instance_values queues;
  try {
    queues = PDH::gather_object_instances("HTTP Service Request Queues", {"CurrentQueueSize", "RejectedRequests", "MaxQueueItemAge"}, false);
  } catch (const PDH::pdh_exception &e) {
    return nscapi::protobuf::functions::set_response_bad(*response, iis_counters_error("HTTP Service Request Queues", e));
  }

  for (const auto &entry : queues) {
    auto obj = std::make_shared<filter_obj>();
    obj->name = entry.first;
    obj->queue_length = static_cast<long long>(PDH::value_of(entry.second, "CurrentQueueSize"));
    obj->rejected = static_cast<long long>(PDH::value_of(entry.second, "RejectedRequests"));
    obj->max_age = static_cast<long long>(PDH::value_of(entry.second, "MaxQueueItemAge"));
    f.match(obj);
  }

  filter_helper.post_process(f);
}

}  // namespace check_iis

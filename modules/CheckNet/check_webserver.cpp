// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_webserver.h"

#include <boost/program_options.hpp>
#include <memory>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

#include "check_http_fetch.hpp"
#include "check_webserver_internal.hpp"

namespace po = boost::program_options;

namespace check_net {

namespace {

using namespace check_webserver_internal;
using check_http_fetch::fetch_options;
using check_http_fetch::fetch_result;
using check_http_fetch::fetch_status_page;

// Fields every status-page filter record shares. The per-check filter_obj
// structs derive from this; register_status_vars/fill_status keep the
// keywords and the fetch_result plumbing in one place.
struct status_obj {
  std::string url;
  std::string host;
  long long port = 0;
  long long code = 0;
  std::string result;
};

template <class THandler>
void register_status_vars(THandler &registry) {
  registry.add_string_var("url", [](auto obj) { return obj->url; }, "Full URL that was requested");
  registry.add_string_var("host", [](auto obj) { return obj->host; }, "Host part of the URL");
  registry.add_string_var("result", [](auto obj) { return obj->result; }, "Result of the check: ok, parse_error, http_<code> or error: <message>");
  registry.add_int_var("port", parsers::where::type_int, [](auto obj) { return obj->port; }, "TCP port that was used");
  registry.add_int_var("code", parsers::where::type_int, [](auto obj) { return obj->code; }, "HTTP status code of the response");
}

void fill_status(status_obj &obj, const std::string &url, const fetch_result &r) {
  obj.url = url;
  obj.host = r.host;
  obj.port = r.port;
  obj.code = r.code;
  obj.result = r.result;
}

// Add the connection options shared by all four checks to the option
// description and wire up the shared defaults.
void add_fetch_options(po::options_description &desc, std::string *url, const std::string &default_url, fetch_options &opt, const std::string &default_ca) {
  opt.ca_file = default_ca;
  // clang-format off
  desc.add_options()
    ("url", po::value<std::string>(url)->default_value(default_url), "URL of the status endpoint (http://host[:port]/path or https://...).")
    ("timeout", po::value<int>(&opt.timeout)->default_value(30)->notifier([](const int value) {
        // Fetching casts this to unsigned; without the check timeout=-1 would
        // silently become a ~136-year timeout instead of an error.
        if (value <= 0) throw po::error("timeout must be a positive number of seconds");
      }), "Connection/read timeout in seconds.")
    ("username", po::value<std::string>(&opt.username), "Username for HTTP Basic authentication.")
    ("password", po::value<std::string>(&opt.password), "Password for HTTP Basic authentication.")
    ("tls-version", po::value<std::string>(&opt.tls_version)->default_value("tlsv1.2+"),
        "TLS version for https (tlsv1.0, tlsv1.1, tlsv1.2, tlsv1.2+, tlsv1.3, sslv3).")
    ("verify", po::value<std::string>(&opt.verify_mode)->default_value("peer"),
        "Certificate verify mode for https: none, peer, peer-cert, fail-if-no-cert, fail-if-no-peer-cert, client-certificate.")
    ("ca", po::value<std::string>(&opt.ca_file)->default_value(default_ca), "Path to a CA bundle used to verify the server certificate.")
    ;
  // clang-format on
}

}  // namespace

// ---------------------------------------------------------------------------
// check_apache_status
// ---------------------------------------------------------------------------

namespace check_apache_filter {

struct filter_obj : status_obj {
  apache_status s;

  std::string show() const { return host + " (" + result + ")"; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler() {
    register_status_vars(registry_);
    registry_.add_string_var("scoreboard", [](auto obj) { return obj->s.scoreboard; }, "The raw mod_status scoreboard string");
    registry_.add_int_var("busy_workers", parsers::where::type_int, [](auto obj) { return obj->s.busy_workers; }, "Workers currently serving requests")
        .add_int_perf("", "", "_busy_workers");
    registry_.add_int_var("idle_workers", parsers::where::type_int, [](auto obj) { return obj->s.idle_workers; }, "Idle (spare) workers")
        .add_int_perf("", "", "_idle_workers");
    registry_
        .add_int_var("total_workers", parsers::where::type_int, [](auto obj) { return obj->s.busy_workers + obj->s.idle_workers; },
                     "Busy plus idle workers (the currently running worker pool)")
        .add_int_perf("", "", "_total_workers");
    registry_.add_int_var("uptime", parsers::where::type_int, [](auto obj) { return obj->s.uptime; }, "Server uptime in seconds").add_int_perf("s", "", "_uptime");
    registry_.add_int_var("total_accesses", parsers::where::type_int, [](auto obj) { return obj->s.total_accesses; }, "Requests served since start")
        .add_int_perf("c", "", "_total_accesses");
    registry_.add_int_var("total_kbytes", parsers::where::type_int, [](auto obj) { return obj->s.total_kbytes; }, "kBytes served since start")
        .add_int_perf("KB", "", "_total_kbytes");
    registry_.add_float("requests_per_sec", [](auto obj) { return obj->s.requests_per_sec; }, "Average requests per second since start")
        .add_float_perf("", "", "_requests_per_sec");
    registry_.add_float("bytes_per_sec", [](auto obj) { return obj->s.bytes_per_sec; }, "Average bytes per second since start")
        .add_float_perf("B", "", "_bytes_per_sec");
  }
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace check_apache_filter

void check_apache_status(const std::string &default_ca_file, const PB::Commands::QueryRequestMessage::Request &request,
                         PB::Commands::QueryResponseMessage::Response *response) {
  using check_apache_filter::filter;
  using check_apache_filter::filter_obj;

  modern_filter::data_container data;
  modern_filter::cli_helper<filter> filter_helper(request, response, data);
  std::string url;
  fetch_options opt;

  filter f;
  filter_helper.add_options("", "result != 'ok'", "", f.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${list}", "${result}: ${busy_workers} busy and ${idle_workers} idle workers, ${requests_per_sec} req/s, uptime ${uptime}s",
                           "${host}", "No status page fetched", "");
  add_fetch_options(filter_helper.get_desc(), &url, "http://127.0.0.1/server-status", opt, default_ca_file);

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(f)) return;
  f.add_manual_perf("busy_workers");
  f.add_manual_perf("idle_workers");
  f.add_manual_perf("requests_per_sec");

  // mod_status only emits the machine-readable format with ?auto; append it
  // for the user so a bare .../server-status URL works.
  const fetch_result r = fetch_status_page(ensure_query_param(url, "auto"), opt);
  auto obj = std::make_shared<filter_obj>();
  fill_status(*obj, url, r);
  if (obj->result == "ok" && !parse_apache_auto(r.body, obj->s)) obj->result = "parse_error";
  f.match(obj);

  filter_helper.post_process(f);
}

// ---------------------------------------------------------------------------
// check_nginx_status
// ---------------------------------------------------------------------------

namespace check_nginx_filter {

struct filter_obj : status_obj {
  nginx_status s;

  std::string show() const { return host + " (" + result + ")"; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler() {
    register_status_vars(registry_);
    registry_.add_int_var("active", parsers::where::type_int, [](auto obj) { return obj->s.active; }, "Active client connections (including waiting)")
        .add_int_perf("", "", "_active");
    registry_.add_int_var("reading", parsers::where::type_int, [](auto obj) { return obj->s.reading; }, "Connections where nginx is reading the request")
        .add_int_perf("", "", "_reading");
    registry_.add_int_var("writing", parsers::where::type_int, [](auto obj) { return obj->s.writing; }, "Connections where nginx is writing the response")
        .add_int_perf("", "", "_writing");
    registry_.add_int_var("waiting", parsers::where::type_int, [](auto obj) { return obj->s.waiting; }, "Idle keep-alive connections")
        .add_int_perf("", "", "_waiting");
    registry_.add_int_var("accepts", parsers::where::type_int, [](auto obj) { return obj->s.accepts; }, "Accepted connections since start")
        .add_int_perf("c", "", "_accepts");
    registry_.add_int_var("handled", parsers::where::type_int, [](auto obj) { return obj->s.handled; }, "Handled connections since start")
        .add_int_perf("c", "", "_handled");
    registry_.add_int_var("requests", parsers::where::type_int, [](auto obj) { return obj->s.requests; }, "Requests served since start")
        .add_int_perf("c", "", "_requests");
    registry_
        .add_int_var("dropped", parsers::where::type_int, [](auto obj) { return obj->s.accepts - obj->s.handled; },
                     "Connections accepted but not handled (resource exhaustion) since start")
        .add_int_perf("c", "", "_dropped");
  }
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace check_nginx_filter

void check_nginx_status(const std::string &default_ca_file, const PB::Commands::QueryRequestMessage::Request &request,
                        PB::Commands::QueryResponseMessage::Response *response) {
  using check_nginx_filter::filter;
  using check_nginx_filter::filter_obj;

  modern_filter::data_container data;
  modern_filter::cli_helper<filter> filter_helper(request, response, data);
  std::string url;
  fetch_options opt;

  filter f;
  filter_helper.add_options("", "result != 'ok'", "", f.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${list}", "${result}: ${active} active (${reading} reading, ${writing} writing, ${waiting} waiting)", "${host}",
                           "No status page fetched", "");
  add_fetch_options(filter_helper.get_desc(), &url, "http://127.0.0.1/nginx_status", opt, default_ca_file);

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(f)) return;
  f.add_manual_perf("active");

  const fetch_result r = fetch_status_page(url, opt);
  auto obj = std::make_shared<filter_obj>();
  fill_status(*obj, url, r);
  if (obj->result == "ok" && !parse_nginx_stub(r.body, obj->s)) obj->result = "parse_error";
  f.match(obj);

  filter_helper.post_process(f);
}

// ---------------------------------------------------------------------------
// check_phpfpm_status
// ---------------------------------------------------------------------------

namespace check_phpfpm_filter {

struct filter_obj : status_obj {
  phpfpm_status s;

  std::string show() const { return (s.pool.empty() ? host : s.pool) + " (" + result + ")"; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler() {
    register_status_vars(registry_);
    registry_.add_string_var("pool", [](auto obj) { return obj->s.pool; }, "Name of the FPM pool");
    registry_.add_string_var("process_manager", [](auto obj) { return obj->s.process_manager; }, "Process manager mode (static, dynamic or ondemand)");
    registry_.add_int_var("active_processes", parsers::where::type_int, [](auto obj) { return obj->s.active_processes; }, "Workers currently serving requests")
        .add_int_perf("", "", "_active_processes");
    registry_.add_int_var("idle_processes", parsers::where::type_int, [](auto obj) { return obj->s.idle_processes; }, "Idle (spare) workers")
        .add_int_perf("", "", "_idle_processes");
    registry_.add_int_var("total_processes", parsers::where::type_int, [](auto obj) { return obj->s.total_processes; }, "Total workers in the pool")
        .add_int_perf("", "", "_total_processes");
    registry_
        .add_int_var("max_active_processes", parsers::where::type_int, [](auto obj) { return obj->s.max_active_processes; },
                     "Highest number of simultaneously active workers since start")
        .add_int_perf("", "", "_max_active_processes");
    registry_.add_int_var("listen_queue", parsers::where::type_int, [](auto obj) { return obj->s.listen_queue; }, "Requests currently waiting in the listen queue")
        .add_int_perf("", "", "_listen_queue");
    registry_
        .add_int_var("max_listen_queue", parsers::where::type_int, [](auto obj) { return obj->s.max_listen_queue; },
                     "Highest listen queue length seen since start")
        .add_int_perf("", "", "_max_listen_queue");
    registry_.add_int_var("listen_queue_len", parsers::where::type_int, [](auto obj) { return obj->s.listen_queue_len; }, "Size of the socket listen queue")
        .add_int_perf("", "", "_listen_queue_len");
    registry_
        .add_int_var("max_children_reached", parsers::where::type_int, [](auto obj) { return obj->s.max_children_reached; },
                     "Times the pool hit pm.max_children since start (the pool was saturated)")
        .add_int_perf("c", "", "_max_children_reached");
    registry_.add_int_var("slow_requests", parsers::where::type_int, [](auto obj) { return obj->s.slow_requests; }, "Requests that exceeded request_slowlog_timeout")
        .add_int_perf("c", "", "_slow_requests");
    registry_.add_int_var("accepted_conn", parsers::where::type_int, [](auto obj) { return obj->s.accepted_conn; }, "Connections accepted since start")
        .add_int_perf("c", "", "_accepted_conn");
  }
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace check_phpfpm_filter

void check_phpfpm_status(const std::string &default_ca_file, const PB::Commands::QueryRequestMessage::Request &request,
                         PB::Commands::QueryResponseMessage::Response *response) {
  using check_phpfpm_filter::filter;
  using check_phpfpm_filter::filter_obj;

  modern_filter::data_container data;
  modern_filter::cli_helper<filter> filter_helper(request, response, data);
  std::string url;
  fetch_options opt;

  filter f;
  filter_helper.add_options("listen_queue > 0", "result != 'ok'", "", f.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${list}",
                           "${result}: pool ${pool}: ${active_processes} active, ${idle_processes} idle, ${listen_queue} queued", "${pool}",
                           "No status page fetched", "");
  add_fetch_options(filter_helper.get_desc(), &url, "http://127.0.0.1/status", opt, default_ca_file);

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(f)) return;
  f.add_manual_perf("active_processes");
  f.add_manual_perf("idle_processes");

  const fetch_result r = fetch_status_page(url, opt);
  auto obj = std::make_shared<filter_obj>();
  fill_status(*obj, url, r);
  if (obj->result == "ok" && !parse_phpfpm_status(r.body, obj->s)) obj->result = "parse_error";
  f.match(obj);

  filter_helper.post_process(f);
}

// ---------------------------------------------------------------------------
// check_tomcat_status
// ---------------------------------------------------------------------------

namespace check_tomcat_filter {

// One record per connector; the JVM heap numbers are repeated on every record
// so thresholds can mix connector and memory keywords.
struct filter_obj : status_obj {
  tomcat_connector c;
  long long memory_free = 0;
  long long memory_total = 0;
  long long memory_max = 0;

  std::string show() const { return (c.name.empty() ? host : c.name) + " (" + result + ")"; }

  double thread_usage() const { return c.threads_max > 0 ? 100.0 * static_cast<double>(c.threads_busy) / static_cast<double>(c.threads_max) : 0.0; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler() {
    register_status_vars(registry_);
    registry_.add_string_var("connector", [](auto obj) { return obj->c.name; }, "Name of the connector (e.g. http-nio-8080)");
    registry_.add_int_var("threads_busy", parsers::where::type_int, [](auto obj) { return obj->c.threads_busy; }, "Threads currently serving requests")
        .add_int_perf("", "", "_threads_busy");
    registry_.add_int_var("threads_current", parsers::where::type_int, [](auto obj) { return obj->c.threads_current; }, "Threads currently alive in the pool")
        .add_int_perf("", "", "_threads_current");
    registry_.add_int_var("threads_max", parsers::where::type_int, [](auto obj) { return obj->c.threads_max; }, "Maximum size of the thread pool")
        .add_int_perf("", "", "_threads_max");
    registry_
        .add_float("thread_usage", [](auto obj) { return obj->thread_usage(); },
                   "Busy threads as a percentage of the maximum thread pool size (0 when the pool size is unknown)")
        .add_float_perf("%", "", "_thread_usage");
    registry_.add_int_var("request_count", parsers::where::type_int, [](auto obj) { return obj->c.request_count; }, "Requests served since start")
        .add_int_perf("c", "", "_request_count");
    registry_.add_int_var("error_count", parsers::where::type_int, [](auto obj) { return obj->c.error_count; }, "Requests that ended in an error since start")
        .add_int_perf("c", "", "_error_count");
    registry_.add_int_var("processing_time", parsers::where::type_int, [](auto obj) { return obj->c.processing_time; }, "Total request processing time in ms since start")
        .add_int_perf("ms", "", "_processing_time");
    registry_.add_int_var("max_time", parsers::where::type_int, [](auto obj) { return obj->c.max_time; }, "Slowest request in ms since start")
        .add_int_perf("ms", "", "_max_time");
    registry_.add_int_var("bytes_received", parsers::where::type_int, [](auto obj) { return obj->c.bytes_received; }, "Bytes received since start")
        .add_int_perf("B", "", "_bytes_received");
    registry_.add_int_var("bytes_sent", parsers::where::type_int, [](auto obj) { return obj->c.bytes_sent; }, "Bytes sent since start")
        .add_int_perf("B", "", "_bytes_sent");
    registry_.add_int_var("memory_free", parsers::where::type_int, [](auto obj) { return obj->memory_free; }, "Free JVM heap in bytes")
        .add_int_perf("B", "", "_memory_free");
    registry_.add_int_var("memory_total", parsers::where::type_int, [](auto obj) { return obj->memory_total; }, "Current JVM heap size in bytes")
        .add_int_perf("B", "", "_memory_total");
    registry_.add_int_var("memory_max", parsers::where::type_int, [](auto obj) { return obj->memory_max; }, "Maximum JVM heap size in bytes")
        .add_int_perf("B", "", "_memory_max");
  }
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace check_tomcat_filter

void check_tomcat_status(const std::string &default_ca_file, const PB::Commands::QueryRequestMessage::Request &request,
                         PB::Commands::QueryResponseMessage::Response *response) {
  using check_tomcat_filter::filter;
  using check_tomcat_filter::filter_obj;

  modern_filter::data_container data;
  modern_filter::cli_helper<filter> filter_helper(request, response, data);
  std::string url;
  fetch_options opt;

  filter f;
  filter_helper.add_options("thread_usage > 75", "result != 'ok' or thread_usage > 90", "", f.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${list}", "${connector} ${result}: ${threads_busy}/${threads_max} threads busy", "${connector}",
                           "No connectors found", "");
  add_fetch_options(filter_helper.get_desc(), &url, "http://127.0.0.1:8080/manager/status", opt, default_ca_file);

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(f)) return;
  f.add_manual_perf("threads_busy");

  // The manager only emits XML with ?XML=true; append it for the user so a
  // bare .../manager/status URL works.
  const fetch_result r = fetch_status_page(ensure_query_param(url, "XML=true"), opt);
  tomcat_status status;
  std::string result = r.result;
  if (result == "ok" && !parse_tomcat_status_xml(r.body, status)) result = "parse_error";
  // A body with zero <connector> elements (e.g. a truncated manager response)
  // never carried the data this check reports; treat it like a parse failure
  // rather than reporting a degenerate 0/0-threads record as OK.
  if (result == "ok" && status.connectors.empty()) result = "parse_error";

  if (result == "ok") {
    for (const tomcat_connector &c : status.connectors) {
      auto obj = std::make_shared<filter_obj>();
      fill_status(*obj, url, r);
      obj->c = c;
      obj->memory_free = status.memory_free;
      obj->memory_total = status.memory_total;
      obj->memory_max = status.memory_max;
      f.match(obj);
    }
  } else {
    // Fetch or parse failure: emit a single record carrying the failure so
    // the default critical expression fires.
    auto obj = std::make_shared<filter_obj>();
    fill_status(*obj, url, r);
    obj->result = result;
    obj->memory_free = status.memory_free;
    obj->memory_total = status.memory_total;
    obj->memory_max = status.memory_max;
    f.match(obj);
  }

  filter_helper.post_process(f);
}

}  // namespace check_net

// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_webserver.h"

#include <check/duration_keyword.hpp>
#include <boost/program_options.hpp>
#include <check/duration_keyword.hpp>
#include <memory>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <str/format.hpp>

#include "check_http_fetch.hpp"
#include "check_jvm_internal.hpp"
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
    // Seconds; takes durations (`uptime < 10m`).
    registry_
        .add_int_var(
            "uptime", parsers::where::type_custom_int_1, [](auto obj) { return obj->s.uptime; }, "Server uptime in seconds; takes durations, e.g. uptime < 10m")
        .add_int_perf("s", "", "_uptime");
    registry_.add_converter(parsers::where::type_custom_int_1, &duration_keyword::parse_duration<std::shared_ptr<filter_obj> >);
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
  filter_helper.add_short_threshold("listen_queue", modern_filter::shorthand::above, modern_filter::shorthand::unit_plain, "0", "");
  filter_helper.add_short_critical("${listen_queue} or result != 'ok'");
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
    registry_.add_int_var("processing_time", duration_keyword::type_milliseconds, [](auto obj) { return obj->c.processing_time; }, "Total request processing time in ms since start")
        .add_int_perf("ms", "", "_processing_time");
    registry_.add_int_var("max_time", duration_keyword::type_milliseconds, [](auto obj) { return obj->c.max_time; }, "Slowest request in ms since start")
        .add_int_perf("ms", "", "_max_time");
    registry_.add_converter(duration_keyword::type_milliseconds, &duration_keyword::parse_duration_ms<std::shared_ptr<filter_obj>>);
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
  filter_helper.add_short_threshold("thread_usage", modern_filter::shorthand::above, modern_filter::shorthand::unit_percent_number, "75", "90");
  filter_helper.add_short_critical("result != 'ok' or ${thread_usage}");
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

// ---------------------------------------------------------------------------
// check_jvm
// ---------------------------------------------------------------------------

namespace check_jvm_filter {

using check_jvm_internal::custom_value;
using check_jvm_internal::jvm_read;
using check_jvm_internal::jvm_status;

struct filter_obj : status_obj {
  jvm_status s;

  std::string show() const { return host + " (" + result + ")"; }

  double heap_usage() const { return s.heap_usage(); }
  std::string heap_human(parsers::where::evaluation_context context) const {
    const std::string used = str::format::format_byte_units(static_cast<double>(s.heap_used), context->get_number_format());
    if (s.heap_max <= 0) return used;
    return used + " of " + str::format::format_byte_units(static_cast<double>(s.heap_max), context->get_number_format());
  }
  // Empty for an attribute that was not read or is not a number, so a
  // threshold over it is false rather than tested against a stand-in 0.
  boost::optional<double> custom_number(const std::string &alias) const {
    const auto it = s.custom.find(alias);
    if (it == s.custom.end() || !it->second.has_number) return boost::none;
    return it->second.number;
  }
  std::string custom_text(const std::string &alias) const {
    const auto it = s.custom.find(alias);
    return it == s.custom.end() ? unread_value : it->second.text;
  }
  static constexpr const char *unread_value = "unread";
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler() {
    register_status_vars(registry_);
    registry_.add_string_var("vm_name", [](auto obj) { return obj->s.vm_name; }, "Name of the JVM implementation (e.g. OpenJDK 64-Bit Server VM)");
    registry_.add_string_var("vm_vendor", [](auto obj) { return obj->s.vm_vendor; }, "Vendor of the JVM implementation");
    registry_.add_string_var("vm_version", [](auto obj) { return obj->s.vm_version; }, "Version of the JVM implementation (e.g. 21.0.5+11-LTS)");
    registry_.add_string_var("spec_version", [](auto obj) { return obj->s.spec_version; }, "Java specification version the JVM implements (e.g. 21, or 1.8 for Java 8)");
    registry_.add_string_var("collectors", [](auto obj) { return obj->s.collector_names(); }, "Names of the garbage collectors, comma separated");
    registry_.add_string_var_w_context("heap", [](auto obj, auto context) { return obj->heap_human(context); },
                                       "Heap in use as human readable text, e.g. 512MB of 4GB (display only; threshold on heap_used or heap_usage)");
    registry_
        .add_int_var("heap_used", parsers::where::type_size, [](auto obj) { return obj->s.heap_used; },
                     "Heap in use in bytes (takes size units, e.g. heap_used > 2G)")
        .add_int_perf("B", "", "_heap_used");
    registry_
        .add_int_var("heap_committed", parsers::where::type_size, [](auto obj) { return obj->s.heap_committed; },
                     "Heap the JVM has committed (reserved from the OS) in bytes")
        .add_int_perf("B", "", "_heap_committed");
    registry_
        .add_int_var("heap_max", parsers::where::type_size, [](auto obj) { return obj->s.heap_max; },
                     "Maximum heap size in bytes (-Xmx); -1 when the JVM has no defined maximum")
        .add_int_perf("B", "", "_heap_max");
    registry_
        .add_float("heap_usage", [](auto obj) { return obj->heap_usage(); },
                   "Heap in use as a percentage of the maximum heap (of the committed heap when there is no maximum)")
        .add_float_perf("%", "", "_heap_usage");
    registry_
        .add_int_var("nonheap_used", parsers::where::type_size, [](auto obj) { return obj->s.nonheap_used; },
                     "Non-heap memory (metaspace, code cache) in use in bytes")
        .add_int_perf("B", "", "_nonheap_used");
    registry_
        .add_int_var("nonheap_committed", parsers::where::type_size, [](auto obj) { return obj->s.nonheap_committed; },
                     "Non-heap memory the JVM has committed in bytes")
        .add_int_perf("B", "", "_nonheap_committed");
    registry_.add_int_var("threads", parsers::where::type_int, [](auto obj) { return obj->s.threads; }, "Live threads (daemon and non-daemon)")
        .add_int_perf("", "", "_threads");
    registry_.add_int_var("daemon_threads", parsers::where::type_int, [](auto obj) { return obj->s.daemon_threads; }, "Live daemon threads")
        .add_int_perf("", "", "_daemon_threads");
    registry_.add_int_var("peak_threads", parsers::where::type_int, [](auto obj) { return obj->s.peak_threads; }, "Highest number of live threads since the JVM started")
        .add_int_perf("", "", "_peak_threads");
    registry_.add_int_var("loaded_classes", parsers::where::type_int, [](auto obj) { return obj->s.loaded_classes; }, "Classes currently loaded")
        .add_int_perf("", "", "_loaded_classes");
    registry_
        .add_int_var("gc_count", parsers::where::type_int, [](auto obj) { return obj->s.gc_count(); },
                     "Garbage collections since the JVM started, summed over all collectors")
        .add_int_perf("c", "", "_gc_count");
    registry_
        .add_int_var("gc_time", duration_keyword::type_milliseconds, [](auto obj) { return obj->s.gc_time(); },
                     "Time spent in garbage collection since the JVM started in ms, summed over all collectors; takes durations, e.g. gc_time > 10m")
        .add_int_perf("ms", "", "_gc_time");
    registry_.add_converter(duration_keyword::type_milliseconds, &duration_keyword::parse_duration_ms<std::shared_ptr<filter_obj> >);
    registry_
        .add_int_var(
            "uptime", parsers::where::type_custom_int_1, [](auto obj) { return obj->s.uptime_ms / 1000; },
            "Time since the JVM started in seconds; takes durations, e.g. uptime < 10m")
        .add_int_perf("s", "", "_uptime");
    registry_.add_converter(parsers::where::type_custom_int_1, &duration_keyword::parse_duration<std::shared_ptr<filter_obj> >);
  }
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

// Keyword names the filter engine reserves for its generic summary (see
// generic_summary in filter_handler_impl.hpp); an alias by one of these names
// would resolve to the summary value in top-syntax.
bool is_reserved_keyword(const std::string &alias) {
  static const char *const reserved[] = {"count",     "total",    "ok_count", "warn_count", "crit_count",  "problem_count", "list",
                                         "ok_list",   "warn_list", "crit_list", "problem_list", "detail_list", "sep",           "status"};
  for (const char *name : reserved)
    if (alias == name) return true;
  return false;
}

}  // namespace check_jvm_filter

void check_jvm(const std::string &default_ca_file, const PB::Commands::QueryRequestMessage::Request &request,
               PB::Commands::QueryResponseMessage::Response *response) {
  using check_jvm_filter::filter;
  using check_jvm_filter::filter_obj;
  using check_jvm_internal::jvm_read;

  modern_filter::data_container data;
  modern_filter::cli_helper<filter> filter_helper(request, response, data);
  std::string url;
  fetch_options opt;
  std::vector<std::string> attributes_raw;

  filter f;
  filter_helper.add_options("heap_usage > 80", "result != 'ok' or heap_usage > 90", "", f.get_filter_syntax(), "unknown");
  filter_helper.add_short_threshold("heap_usage", modern_filter::shorthand::above, modern_filter::shorthand::unit_percent_number, "80", "90");
  filter_helper.add_short_critical("result != 'ok' or ${heap_usage}");
  filter_helper.add_syntax("${status}: ${list}", "${result}: heap ${heap} (${heap_usage}%), ${threads} threads, ${gc_count} collections", "${host}",
                           "No JVM queried", "");
  add_fetch_options(filter_helper.get_desc(), &url, "http://127.0.0.1:8778/jolokia/", opt, default_ca_file);
  // clang-format off
  filter_helper.get_desc().add_options()
    ("attribute", po::value<std::vector<std::string> >(&attributes_raw),
        "Read an MBean attribute as a filter keyword: 'alias=mbean/attribute[/inner/path]' (repeatable). "
        "Write a / inside the MBean name as !/ (Jolokia's escape). Numeric values compare as numbers and become perfdata; "
        "example: attribute=sessions=Catalina:type=Manager,host=localhost,context=!/app/activeSessions \"warn=sessions > 500\".")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  std::vector<jvm_read> reads;
  for (const std::string &raw : attributes_raw) {
    jvm_read r;
    std::string error;
    if (!check_jvm_internal::parse_read_spec(raw, r, error)) return nscapi::protobuf::functions::set_response_bad(*response, "Invalid attribute: " + error);
    if (check_jvm_filter::is_reserved_keyword(r.alias) || f.context->registry_.has_variable(r.alias))
      return nscapi::protobuf::functions::set_response_bad(*response, "Invalid attribute: the alias " + r.alias + " is already a keyword of check_jvm");
    for (const jvm_read &other : reads)
      if (other.alias == r.alias) return nscapi::protobuf::functions::set_response_bad(*response, "Invalid attribute: the alias " + r.alias + " is used twice");
    reads.push_back(r);
  }

  // Each alias becomes an optional number on this check's own handler:
  // numeric comparisons use the value with its decimals and are false when
  // the attribute was not read or is not a number, string comparisons and
  // rendering use the raw value ("unread" when there is none), and a numeric
  // value is float perfdata (<host>_<alias>). Registered before build_filter
  // parses the expressions that use them.
  for (const jvm_read &r : reads) {
    const std::string alias = r.alias;
    std::string description = "MBean attribute " + r.mbean + "/" + r.attribute;
    if (!r.path.empty()) description += "/" + r.path;
    f.context->registry_.add_optional_float_var(
        alias, [alias](std::shared_ptr<filter_obj> o) { return o->custom_number(alias); },
        [alias](std::shared_ptr<filter_obj> o) { return o->custom_text(alias); }, description);
  }

  if (!filter_helper.build_filter(f)) return;
  f.add_manual_perf("heap_used");
  f.add_manual_perf("threads");
  f.add_manual_perf("gc_count");
  f.add_manual_perf("gc_time");

  // One bulk POST carries every read; Jolokia answers them in order, each
  // with its own status, so one missing MBean does not hide the others.
  opt.post_body = check_jvm_internal::build_bulk_request(reads);
  const fetch_result r = fetch_status_page(url, opt);
  auto obj = std::make_shared<filter_obj>();
  fill_status(*obj, url, r);
  if (obj->result == "ok") {
    const std::string agent_error = check_jvm_internal::top_level_error(r.body);
    if (!agent_error.empty()) {
      obj->result = "error: " + agent_error;
    } else if (!check_jvm_internal::parse_bulk_response(r.body, reads, obj->s)) {
      obj->result = "parse_error";
    } else if (!obj->s.error.empty()) {
      obj->result = "error: " + obj->s.error;
    }
  }
  f.match(obj);

  filter_helper.post_process(f);
}

}  // namespace check_net

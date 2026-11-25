// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// check_jvm: its own translation unit rather than a fifth filter in
// check_webserver.cpp, which overflowed MSVC's section limit (C1128).

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
#include "check_webserver.h"
#include "check_webserver_common.hpp"

namespace po = boost::program_options;

namespace check_net {

using namespace check_webserver_common;
using check_http_fetch::fetch_status_page;

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
    // The handler's own check covers both this check's keywords and the
    // generic summary names (count, list, lines, status, ...), so the list
    // cannot drift from the engine's.
    if (f.context->has_variable(r.alias))
      return nscapi::protobuf::functions::set_response_bad(
          *response, "Invalid attribute: the alias " + r.alias + " is already a keyword of check_jvm or a generic summary name (count, list, status, ...)");
    for (const jvm_read &other : reads)
      if (other.alias == r.alias) return nscapi::protobuf::functions::set_response_bad(*response, "Invalid attribute: the alias " + r.alias + " is used twice");
    reads.push_back(r);
  }

  // Each alias becomes an optional decimal on this check's own handler:
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
  // No HTTP response at all (connection refused, timeout, DNS failure, or a
  // URL that could not be parsed): there is no JVM state to report, so the
  // check is UNKNOWN with no perfdata rather than a record of zeros. An HTTP
  // error or a Jolokia error means the server answered; those stay records,
  // with `result` set, for the thresholds to judge.
  if (r.code == 0) return nscapi::protobuf::functions::set_response_bad(*response, url + ": " + r.result);
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

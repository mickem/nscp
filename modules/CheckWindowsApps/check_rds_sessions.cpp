// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_rds_sessions.hpp"

#include <boost/algorithm/string/trim.hpp>
#include <boost/program_options.hpp>
#include <map>
#include <memory>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>
#include <vector>
#include <win/pdh/pdh_enumerations.hpp>
#include <win/pdh/pdh_interface.hpp>
#include <win/pdh/pdh_object_gather.hpp>

#include "check_rds_internal.hpp"

namespace po = boost::program_options;

namespace check_rds {

using check_rds_internal::is_session_instance;

namespace {

std::string counters_error(const std::string &object, const PDH::pdh_exception &e) {
  // PDH messages come from FormatMessage and end in \r\n; trim so the check
  // output stays a single line.
  std::string reason = e.reason();
  boost::algorithm::trim(reason);
  return "Remote Desktop Services counters (" + object + ") not available - is the role installed on this host? (" + reason + ")";
}

using PDH::value_of;

}  // namespace

// ---------------------------------------------------------------------------
// check_rds_sessions — session counts on a session host
// ---------------------------------------------------------------------------

namespace rds_sessions_filter {

struct filter_obj {
  long long active = 0;
  long long inactive = 0;
  long long total = 0;

  std::string show() const { return std::to_string(active) + " active, " + std::to_string(inactive) + " inactive"; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler() {
    registry_.add_int_var("active", parsers::where::type_int, [](auto obj) { return obj->active; }, "Sessions with a connected user")
        .add_int_perf("", "", "_active");
    registry_.add_int_var("inactive", parsers::where::type_int, [](auto obj) { return obj->inactive; }, "Disconnected (idle) sessions still holding resources")
        .add_int_perf("", "", "_inactive");
    registry_.add_int_var("total_sessions", parsers::where::type_int, [](auto obj) { return obj->total; }, "Total sessions on the host")
        .add_int_perf("", "", "_total");
  }
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace rds_sessions_filter

void check_rds_sessions(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  using rds_sessions_filter::filter;
  using rds_sessions_filter::filter_obj;

  modern_filter::data_container data;
  modern_filter::cli_helper<filter> filter_helper(request, response, data);

  filter f;
  filter_helper.add_options("", "", "", f.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${list}", "${active} active, ${inactive} inactive (${total_sessions} total)", "sessions", "No session counters found", "");
  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(f)) return;
  f.add_manual_perf("active");
  f.add_manual_perf("inactive");
  f.add_manual_perf("total_sessions");

  std::map<std::string, double> values;
  try {
    values = PDH::gather_object_values("Terminal Services", {"Active Sessions", "Inactive Sessions", "Total Sessions"}, false);
  } catch (const PDH::pdh_exception &e) {
    return nscapi::protobuf::functions::set_response_bad(*response, counters_error("Terminal Services", e));
  }

  auto obj = std::make_shared<filter_obj>();
  obj->active = static_cast<long long>(value_of(values, "Active Sessions"));
  obj->inactive = static_cast<long long>(value_of(values, "Inactive Sessions"));
  obj->total = static_cast<long long>(value_of(values, "Total Sessions"));
  f.match(obj);

  filter_helper.post_process(f);
}

// ---------------------------------------------------------------------------
// check_rds_session_load — per-session resource usage
// ---------------------------------------------------------------------------

namespace rds_session_load_filter {

struct filter_obj {
  std::string session;
  double cpu = 0.0;
  long long working_set = 0;
  long long total_bytes = 0;

  std::string show() const { return session + " (ws " + std::to_string(working_set) + "B)"; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler() {
    registry_.add_string_var("session", [](auto obj) { return obj->session; }, "Counter instance name (Console, Services, RDP-Tcp <n>, ...)");
    registry_.add_float("cpu", [](auto obj) { return obj->cpu; }, "% processor time of the session (needs averages=true, otherwise 0)")
        .add_float_perf("%", "", "_cpu");
    registry_.add_int_var("working_set", parsers::where::type_int, [](auto obj) { return obj->working_set; }, "Working set of the session in bytes")
        .add_int_perf("B", "", "_working_set");
    registry_.add_int_var("total_bytes", parsers::where::type_int, [](auto obj) { return obj->total_bytes; },
                          "Protocol bytes in+out since the session connected")
        .add_int_perf("B", "", "_total_bytes");
  }
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace rds_session_load_filter

void check_rds_session_load(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  using rds_session_load_filter::filter;
  using rds_session_load_filter::filter_obj;

  modern_filter::data_container data;
  modern_filter::cli_helper<filter> filter_helper(request, response, data);
  bool averages = false;
  bool sessions_only = false;

  filter f;
  filter_helper.add_options("", "", "", f.get_filter_syntax(), "ok");
  filter_helper.add_syntax("${status}: ${list}", "${session}: ${cpu}% cpu, ${working_set}B working set", "${session}", "No sessions found", "");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("averages", po::value<bool>(&averages)->implicit_value(true)->default_value(false),
        "Collect a second sample after one second so the cpu keyword carries a real value.")
    ("sessions-only", po::value<bool>(&sessions_only)->implicit_value(true)->default_value(false),
        "Skip the 'Services' aggregate instance (session 0 / system processes) and report only console/RDP sessions.")
    ;
  // clang-format on
  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(f)) return;
  f.add_manual_perf("working_set");

  PDH::object_instance_values sessions;
  try {
    sessions = PDH::gather_object_instances("Terminal Services Session", {"% Processor Time", "Working Set", "Total Bytes"}, averages);
  } catch (const PDH::pdh_exception &e) {
    return nscapi::protobuf::functions::set_response_bad(*response, counters_error("Terminal Services Session", e));
  }

  for (const auto &entry : sessions) {
    if (sessions_only && !is_session_instance(entry.first)) continue;
    auto obj = std::make_shared<filter_obj>();
    obj->session = entry.first;
    obj->cpu = value_of(entry.second, "% Processor Time");
    obj->working_set = static_cast<long long>(value_of(entry.second, "Working Set"));
    obj->total_bytes = static_cast<long long>(value_of(entry.second, "Total Bytes"));
    f.match(obj);
  }

  filter_helper.post_process(f);
}

// ---------------------------------------------------------------------------
// check_rds_broker — Connection Broker counterset
// ---------------------------------------------------------------------------

namespace rds_broker_filter {

struct filter_obj {
  std::string counter;
  std::string instance;
  double value = 0.0;

  // Unique per record: the instanced branch emits one record per counter per
  // instance, so the counter name alone would collide as a perfdata label
  // (graphing backends keep one series and silently drop the rest).
  std::string label() const { return instance.empty() ? counter : counter + "_" + instance; }

  std::string show() const { return counter + (instance.empty() ? "" : " (" + instance + ")") + " = " + std::to_string(static_cast<long long>(value)); }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler() {
    registry_.add_string_var("counter", [](auto obj) { return obj->counter; }, "Name of the broker counter");
    registry_.add_string_var("instance", [](auto obj) { return obj->instance; }, "Counter instance (empty for the single-instance counters)");
    registry_.add_string_var("label", [](auto obj) { return obj->label(); },
                             "Counter name suffixed with the instance name when the counterset is multi-instance (unique per record)");
    registry_.add_numbers("value", parsers::where::type_float, [](auto obj) { return static_cast<long long>(obj->value); },
                          [](auto obj) { return obj->value; }, "Value of the counter")
        .add_int_perf("", "", "");
  }
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace rds_broker_filter

void check_rds_broker(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  using rds_broker_filter::filter;
  using rds_broker_filter::filter_obj;

  const std::string kObject = "Remote Desktop Connection Broker Counterset";

  modern_filter::data_container data;
  modern_filter::cli_helper<filter> filter_helper(request, response, data);
  bool averages = false;

  filter f;
  filter_helper.add_options("", "", "", f.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${list}", "${label} = ${value}", "${label}", "No Connection Broker counters found", "");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("averages", po::value<bool>(&averages)->implicit_value(true)->default_value(false),
        "Collect a second sample after one second so rate counters carry real values.")
    ;
  // clang-format on
  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(f)) return;
  f.add_manual_perf("value");

  // The counterset's counter names vary between Windows versions, so
  // enumerate whatever this broker exposes instead of hard-coding names;
  // filter/thresholds select by the `counter` keyword.
  PDH::Enumerations::Object object = PDH::Enumerations::EnumObject(kObject);
  if (!object.error.empty() || object.counters.empty()) {
    return nscapi::protobuf::functions::set_response_bad(
        *response, "Connection Broker counters (" + kObject + ") not available - is this host an RD Connection Broker?" +
                       (object.error.empty() ? "" : " (" + object.error + ")"));
  }
  const std::vector<std::string> counters(object.counters.begin(), object.counters.end());

  const auto match_instanced = [&f](const PDH::object_instance_values &values) {
    for (const auto &instance : values) {
      for (const auto &entry : instance.second) {
        auto obj = std::make_shared<filter_obj>();
        obj->counter = entry.first;
        obj->instance = instance.first;
        obj->value = entry.second;
        f.match(obj);
      }
    }
  };

  try {
    if (object.instances.empty()) {
      try {
        const std::map<std::string, double> values = PDH::gather_object_values(kObject, counters, averages);
        for (const auto &entry : values) {
          auto obj = std::make_shared<filter_obj>();
          obj->counter = entry.first;
          obj->value = entry.second;
          f.match(obj);
        }
      } catch (const PDH::pdh_exception &) {
        // A multi-instance counterset that momentarily has no instances (e.g.
        // a freshly started, idle broker) also enumerates an empty instance
        // list, and its counter paths fail without an instance specifier.
        // Retry as instanced: an empty result then renders the empty-state
        // message instead of a bogus "is the role installed?" error.
        match_instanced(PDH::gather_object_instances(kObject, counters, averages));
      }
    } else {
      match_instanced(PDH::gather_object_instances(kObject, counters, averages));
    }
  } catch (const PDH::pdh_exception &e) {
    return nscapi::protobuf::functions::set_response_bad(*response, counters_error(kObject, e));
  }

  filter_helper.post_process(f);
}

}  // namespace check_rds

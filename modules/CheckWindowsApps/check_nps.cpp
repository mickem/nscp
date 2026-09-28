// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only
#include "check_nps.hpp"

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <ctime>
#include <memory>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <str/utf8.hpp>

#include "check_nps_internal.hpp"

namespace check_nps {
namespace {
struct auth_record : summary {
  long long min_requests = 20;
  bool require_traffic = false;
  std::string show() const { return group; }
};
struct auth_handler : parsers::where::filter_handler_impl<std::shared_ptr<auth_record>> {
  auth_handler() {
    registry_.add_string_var("group", [](auto o) { return o->group; }, "all, client, policy, or reason-code group");
    registry_.add_string_var("top_reason", [](auto o) { return o->top_reason(); }, "Most frequent reject/discard reason code; none without failures");
    registry_.add_int_var(
                 "accepted", parsers::where::type_int, [](auto o) { return o->accepted; }, "Access-granted events (6272)")
        .add_int_perf("", "", "_accepted");
    registry_.add_int_var(
                 "rejected", parsers::where::type_int, [](auto o) { return o->rejected; }, "Access-denied events (6273)")
        .add_int_perf("", "", "_rejected");
    registry_.add_int_var(
                 "discarded", parsers::where::type_int, [](auto o) { return o->discarded; }, "Discarded authentication events (6274)")
        .add_int_perf("", "", "_discarded");
    registry_.add_int_var("requests", parsers::where::type_int, [](auto o) { return o->requests(); }, "Accepted + rejected + discarded events");
    registry_.add_int_var("decisions", parsers::where::type_int, [](auto o) { return o->decisions(); }, "Accepted + rejected events; percentage denominator");
    registry_
        .add_float(
            "reject_pct", [](auto o) { return o->reject_pct(); }, "100 * rejected / (accepted + rejected), or 0 when no decisions; excludes discards")
        .add_float_perf("%", "", "_reject_pct");
    registry_.add_int_var(
        "min_requests", parsers::where::type_int, [](auto o) { return o->min_requests; }, "Minimum decisions for default percentage thresholds");
    registry_.add_int_var(
        "require_traffic", parsers::where::type_bool, [](auto o) { return o->require_traffic; }, "Whether zero authentication events should alert");
  }
};
using auth_filter = modern_filter::modern_filters<auth_record, auth_handler>;

struct accounting_record {
  long long discards = 0;
  std::string log_state = "not_checked";
  boost::optional<long long> age, size;
  std::string show() const { return "accounting"; }
};
struct accounting_handler : parsers::where::filter_handler_impl<std::shared_ptr<accounting_record>> {
  accounting_handler() {
    registry_.add_int_var(
                 "accounting_discards", parsers::where::type_int, [](auto o) { return o->discards; }, "Discarded accounting requests (event 6275)")
        .add_int_perf("", "", "_discards");
    registry_.add_string_var(
        "log_state", [](auto o) { return o->log_state; }, "not_checked, ok, missing, empty, or stale; freshness/empty checks require require-traffic=true");
    registry_
        .add_optional_int_var(
            "log_age", [](auto o) { return o->age; }, "unknown", "Seconds since the selected log file was modified; unknown when not available")
        .add_int_perf("s", "", "_log_age");
    registry_.add_optional_int_var(
                 "log_size", [](auto o) { return o->size; }, "unknown", "Selected log file size in bytes; unknown when not available")
        .add_int_perf("B", "", "_log_size");
  }
};
using accounting_filter = modern_filter::modern_filters<accounting_record, accounting_handler>;

struct counter_record : counter_value {
  std::string show() const { return label(); }
};
struct counter_handler : parsers::where::filter_handler_impl<std::shared_ptr<counter_record>> {
  counter_handler() {
    registry_.add_string_var("object", [](auto o) { return o->object; }, "Installed performance object name");
    registry_.add_string_var("counter", [](auto o) { return o->counter; }, "Installed counter name");
    registry_.add_string_var("instance", [](auto o) { return o->instance; }, "Counter instance, including _Total; empty for a single-instance object");
    registry_.add_string_var("label", [](auto o) { return o->label(); }, "Object, counter and instance used for a unique performance label");
    registry_.add_float(
                 "value", [](auto o) { return o->value; }, "Formatted PDH value after two samples; units depend on the selected counter")
        .add_float_perf("", "", "");
  }
};
using counter_filter = modern_filter::modern_filters<counter_record, counter_handler>;

bool valid_window(int seconds, int max_events, PB::Commands::QueryResponseMessage::Response *response) {
  if (seconds >= 1 && seconds <= 86400 && max_events >= 1 && max_events <= 1000000) return true;
  nscapi::protobuf::functions::set_response_bad(*response, "window must be 1..86400 seconds and max-events 1..1000000");
  return false;
}
}  // namespace

void check_nps_auth(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  namespace po = boost::program_options;
  modern_filter::data_container data;
  modern_filter::cli_helper<auth_filter> helper(request, response, data);
  auth_filter filter;
  int seconds = 300, max_events = 100000, minimum = 20;
  bool require_traffic = false;
  std::string group_by;
  helper.add_options("decisions >= min_requests and reject_pct > 10",
                     "discarded > 0 or (decisions >= min_requests and reject_pct > 25) or (require_traffic = 1 and requests = 0)", "",
                     filter.get_filter_syntax(), "unknown");
  helper.add_syntax("${status}: ${list}",
                    "${group}: ${accepted} accepted, ${rejected} rejected, ${discarded} discarded, reject=${reject_pct}%, top reason=${top_reason}", "${group}",
                    "No NPS groups found in window", "");
  // clang-format off
  helper.get_desc().add_options()
    ("window", po::value<int>(&seconds)->default_value(300), "Scan the last N seconds (1..86400), with a fixed start and end time.")
    ("max-events", po::value<int>(&max_events)->default_value(100000), "Fail UNKNOWN instead of reporting partial counts if this many events is exceeded.")
    ("min-requests", po::value<int>(&minimum)->default_value(20), "Minimum accepted+rejected events per group before default percentage thresholds apply.")
    ("group-by", po::value<std::string>(&group_by)->default_value("all"), "Aggregate by all, client, policy, or reason (numeric reason code).")
    ("require-traffic", po::value<bool>(&require_traffic)->implicit_value(true)->default_value(false), "Alert on a quiet aggregate window; only supported with group-by=all.");
  // clang-format on
  if (!helper.parse_options()) return;
  if (!helper.build_filter(filter)) return;
  if (!valid_window(seconds, max_events, response)) return;
  if (minimum < 1) return nscapi::protobuf::functions::set_response_bad(*response, "min-requests must be positive");
  if (require_traffic && group_by != "all") return nscapi::protobuf::functions::set_response_bad(*response, "require-traffic requires group-by=all");
  try {
    summarize({}, group_by);  // Validate options before touching Windows APIs.
    const auto summaries = summarize(read_events(seconds, max_events, false), group_by);
    for (const auto &name : {"accepted", "rejected", "discarded", "reject_pct"}) filter.add_manual_perf(name);
    for (const auto &summary : summaries) {
      auto item = std::make_shared<auth_record>();
      static_cast<check_nps::summary &>(*item) = summary;
      item->min_requests = minimum;
      item->require_traffic = require_traffic;
      filter.match(item);
    }
    helper.post_process(filter);
  } catch (const std::exception &e) {
    nscapi::protobuf::functions::set_response_bad(*response, std::string("NPS authentication data unavailable: ") + e.what());
  }
}

void check_nps_accounting(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  namespace po = boost::program_options;
  modern_filter::data_container data;
  modern_filter::cli_helper<accounting_filter> helper(request, response, data);
  accounting_filter filter;
  int seconds = 300, max_events = 100000, max_age = 600;
  bool require_traffic = false;
  std::string log_file;
  helper.add_options("", "accounting_discards > 0 or log_state in ('missing', 'empty', 'stale')", "", filter.get_filter_syntax(), "unknown");
  helper.add_syntax("${status}: ${list}", "${accounting_discards} accounting discards, log=${log_state}, age=${log_age}, size=${log_size}", "nps",
                    "No NPS accounting data", "");
  // clang-format off
  helper.get_desc().add_options()
    ("window", po::value<int>(&seconds)->default_value(300), "Scan the last N seconds for discarded accounting requests.")
    ("max-events", po::value<int>(&max_events)->default_value(100000), "Maximum events; exceeding the limit returns UNKNOWN.")
    ("log-file", po::value<std::string>(&log_file), "Optional current accounting log file to inspect. Supply the current path after rotation; SQL logging is checked separately with CheckMSSQL.")
    ("require-traffic", po::value<bool>(&require_traffic)->implicit_value(true)->default_value(false), "Check the selected file for empty/stale output when accounting traffic is expected.")
    ("max-age", po::value<int>(&max_age)->default_value(600), "Maximum log age in seconds when require-traffic=true.");
  // clang-format on
  if (!helper.parse_options()) return;
  if (!helper.build_filter(filter)) return;
  if (!valid_window(seconds, max_events, response)) return;
  if (max_age < 1 || (require_traffic && log_file.empty()))
    return nscapi::protobuf::functions::set_response_bad(*response, "max-age must be positive; require-traffic needs log-file");
  try {
    auto item = std::make_shared<accounting_record>();
    item->discards = static_cast<long long>(read_events(seconds, max_events, true).size());
    if (!log_file.empty()) {
      const boost::filesystem::path path(utf8::cvt<std::wstring>(log_file));
      if (!boost::filesystem::exists(path))
        item->log_state = "missing";
      else {
        item->size = static_cast<long long>(boost::filesystem::file_size(path));
        item->age = std::max<long long>(0, static_cast<long long>(std::time(nullptr) - boost::filesystem::last_write_time(path)));
        item->log_state = require_traffic && item->size.value() == 0 ? "empty" : require_traffic && item->age.value() > max_age ? "stale" : "ok";
      }
    }
    for (const auto &name : {"accounting_discards", "log_age", "log_size"}) filter.add_manual_perf(name);
    filter.match(item);
    helper.post_process(filter);
  } catch (const std::exception &e) {
    nscapi::protobuf::functions::set_response_bad(*response, std::string("NPS accounting data unavailable: ") + e.what());
  }
}

void check_nps_counters(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  namespace po = boost::program_options;
  modern_filter::data_container data;
  modern_filter::cli_helper<counter_filter> helper(request, response, data);
  counter_filter filter;
  std::string object;
  int sample_ms = 1000;
  helper.add_options("", "", "", filter.get_filter_syntax(), "unknown");
  helper.add_syntax("${status}: ${list}", "${label}=${value}", "${label}", "No NPS counters found", "");
  // clang-format off
  helper.get_desc().add_options()
    ("object", po::value<std::string>(&object)->default_value("NPS Authentication Server"), "Installed NPS performance object; use NPS Accounting Server for accounting, or a localized name.")
    ("sample-ms", po::value<int>(&sample_ms)->default_value(1000), "Delay between the two PDH samples (100..10000 milliseconds).");
  // clang-format on
  if (!helper.parse_options()) return;
  if (!helper.build_filter(filter)) return;
  if (object.empty() || sample_ms < 100 || sample_ms > 10000)
    return nscapi::protobuf::functions::set_response_bad(*response, "object is required and sample-ms must be 100..10000");
  try {
    filter.add_manual_perf("value");
    for (const auto &value : read_counters(object, sample_ms)) {
      auto item = std::make_shared<counter_record>();
      static_cast<counter_value &>(*item) = value;
      filter.match(item);
    }
    helper.post_process(filter);
  } catch (const std::exception &e) {
    nscapi::protobuf::functions::set_response_bad(*response, std::string("NPS counters unavailable: ") + e.what());
  }
}
}  // namespace check_nps

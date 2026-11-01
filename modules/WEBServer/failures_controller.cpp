// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "failures_controller.hpp"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/date_time/c_local_time_adjustor.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/json.hpp>
#include <ctime>
#include <list>
#include <metrics/metrics_json.hpp>
#include <nscapi/protobuf/failures.hpp>
#include <nscapi/protobuf/failures_json.hpp>
#include <set>
#include <str/utils.hpp>
#include <str/xtos.hpp>
#include <string>
#include <utility>

namespace json = boost::json;

namespace {
// Local time, matching the `date` field the log, event and result endpoints
// emit. The epoch integer travels alongside it, so a client never parses this.
std::string to_date(const std::int64_t timestamp) {
  if (timestamp <= 0) return "";
  const boost::posix_time::ptime utc = boost::posix_time::from_time_t(static_cast<std::time_t>(timestamp));
  return boost::posix_time::to_simple_string(boost::date_time::c_local_adjustor<boost::posix_time::ptime>::utc_to_local(utc));
}

// Accepts both the words and the Nagios numbers, so `?status=critical` and
// `?status=2` are the same query - the same vocabulary /api/v2/results takes.
// Returns false on anything else rather than silently matching everything.
bool parse_status(const std::string &token, PB::Common::ResultCode &status) {
  std::string value = token;
  boost::algorithm::trim(value);
  boost::algorithm::to_lower(value);
  if (value == "ok" || value == "0")
    status = PB::Common::ResultCode::OK;
  else if (value == "warning" || value == "warn" || value == "1")
    status = PB::Common::ResultCode::WARNING;
  else if (value == "critical" || value == "crit" || value == "2")
    status = PB::Common::ResultCode::CRITICAL;
  else if (value == "unknown" || value == "3")
    status = PB::Common::ResultCode::UNKNOWN;
  else
    return false;
  return true;
}

// A non-negative integer query parameter; false (and a 400 from the caller)
// on anything else, so a typo in `since` does not quietly mean "everything".
bool parse_unsigned(const std::string &text, unsigned long long &out) {
  if (text.empty()) return false;
  for (const char c : text) {
    if (c < '0' || c > '9') return false;
  }
  try {
    out = std::stoull(text);
  } catch (const std::exception &) {
    return false;
  }
  return true;
}

// The shared rendering (failures_json.hpp) plus what only REST carries: the
// local `date` and the `age` at the time of the request.
json::object to_json(const PB::Failures::FailedCheck &entry, const std::int64_t now, const bool arguments_recorded) {
  json::object node = failures_json::entry(entry, arguments_recorded);
  node["date"] = to_date(entry.timestamp());
  node["age"] = now > entry.timestamp() ? now - entry.timestamp() : 0;
  return node;
}

// The envelope's configuration fields, shared by GET and DELETE so a client
// that empties the log learns the same things about it as one that reads it.
void describe(json::object &out, const PB::Failures::FailuresResponseMessage::Response &payload) {
  out["enabled"] = payload.enabled();
  out["max_entries"] = static_cast<std::int64_t>(payload.max_entries());
  out["arguments_recorded"] = payload.arguments_recorded();
  out["statuses"] = payload.statuses();
  out["last_index"] = static_cast<std::int64_t>(payload.last_index());
  out["dropped"] = static_cast<std::int64_t>(payload.dropped());
  out["capture_metrics"] = payload.capture_metrics();
  out["context_budget"] = static_cast<std::int64_t>(payload.context_budget());
  out["max_context_size"] = static_cast<std::int64_t>(payload.max_context_size());
  out["context_bytes_used"] = static_cast<std::int64_t>(payload.context_bytes_used());
}

// A core that predates the API answers with nothing; render that as a log
// that is switched off, which is what it amounts to for the caller.
bool parse_envelope(const std::string &envelope, PB::Failures::FailuresResponseMessage &message) {
  return !envelope.empty() && message.ParseFromString(envelope) && message.payload_size() > 0;
}
}  // namespace

failures_controller::failures_controller(const int version, std::shared_ptr<session_manager_interface> session, const nscapi::core_wrapper *core,
                                         const unsigned int plugin_id)
    : RegexpController(version == 1 ? "/api/v1/failures" : "/api/v2/failures"), session(std::move(session)), core(core), plugin_id(plugin_id) {
  addRoute("GET", "/?$", this, &failures_controller::list_failures);
  addRoute("DELETE", "/?$", this, &failures_controller::clear_failures);
  addRoute("GET", "/([0-9]+)/context/?$", this, &failures_controller::get_context);
}

void failures_controller::list_failures(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("failures.list", request, response)) return;

  unsigned long long since = 0;
  unsigned long long limit = 0;
  const std::string since_text = request.get("since", "");
  if (!since_text.empty() && !parse_unsigned(since_text, since)) {
    response.setCodeBadRequest("Invalid since: " + since_text);
    return;
  }
  const std::string limit_text = request.get("limit", "");
  if (!limit_text.empty() && !parse_unsigned(limit_text, limit)) {
    response.setCodeBadRequest("Invalid limit: " + limit_text);
    return;
  }

  std::set<int> statuses;
  const std::string status_text = request.get("status", "");
  if (!status_text.empty()) {
    for (const std::string &token : str::utils::split<std::list<std::string> >(status_text, ",")) {
      PB::Common::ResultCode status = PB::Common::ResultCode::OK;
      if (!parse_status(token, status)) {
        response.setCodeBadRequest("Invalid status filter: " + token);
        return;
      }
      statuses.insert(failures_json::status_number(status));
    }
  }
  const std::string command = request.get("command", "");

  // `since` is positional and the store applies it; `limit` is too, but only
  // when nothing else narrows the list - a filtered page has to be cut after
  // the filter or "the 10 latest critical ones" would be "the critical ones
  // among the 10 latest".
  const bool filtered = !statuses.empty() || !command.empty();
  PB::Failures::FailuresResponseMessage message;
  const bool known = parse_envelope(core->get_failures(since, filtered ? 0 : static_cast<unsigned int>(limit)), message);

  json::object out;
  json::array entries;
  const std::int64_t now = static_cast<std::int64_t>(std::time(nullptr));
  if (known) {
    const PB::Failures::FailuresResponseMessage::Response &payload = message.payload(0);
    describe(out, payload);
    for (const PB::Failures::FailedCheck &entry : payload.entries()) {
      if (!statuses.empty() && statuses.find(failures_json::status_number(entry.response().result())) == statuses.end()) continue;
      if (!command.empty() && !boost::algorithm::iequals(command, entry.response().command()) && !boost::algorithm::iequals(command, entry.response().alias()))
        continue;
      if (limit > 0 && entries.size() >= limit) break;
      entries.push_back(to_json(entry, now, payload.arguments_recorded()));
    }
  } else {
    out["enabled"] = false;
    out["max_entries"] = 0;
    out["arguments_recorded"] = false;
    out["statuses"] = "";
    out["last_index"] = 0;
    out["dropped"] = 0;
    out["capture_metrics"] = false;
    out["context_budget"] = 0;
    out["max_context_size"] = 0;
    out["context_bytes_used"] = 0;
  }
  out["count"] = static_cast<std::int64_t>(entries.size());
  out["entries"] = entries;
  response.setHeader("X-Result-Count", str::xtos(entries.size()));
  response.append(json::serialize(out));
}

// The metrics snapshot one entry was recorded with, rendered exactly as
// /api/v2/metrics renders the live one (`metrics` flat, `metadata` keyed the
// same), so a client reads both with one parser. An entry that has no
// snapshot - capture was off, it was evicted, it was too large - answers 200
// with its state and no `metrics`: the entry exists, there is just nothing
// to show. An entry the log no longer holds is a 404.
void failures_controller::get_context(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("failures.list", request, response)) return;
  unsigned long long index = 0;
  if (!parse_unsigned(what.str(1), index) || index == 0) {
    response.setCodeBadRequest("Invalid index: " + what.str(1));
    return;
  }
  const bool with_meta = request.get("meta", "1") != "0";

  PB::Failures::FailuresResponseMessage message;
  if (!parse_envelope(core->get_failure_context(index), message) || message.payload(0).entries_size() == 0) {
    response.setCodeNotFound("Failed check not found: " + what.str(1));
    return;
  }
  const PB::Failures::FailedCheck &entry = message.payload(0).entries(0);
  json::object out = failures_json::context(entry);
  out["index"] = static_cast<std::int64_t>(entry.index());
  out["timestamp"] = entry.timestamp();
  if (entry.context().state() == PB::Failures::MetricsContext::CAPTURED && entry.context().has_metrics()) {
    json::object metrics_list, metrics_meta;
    metrics_json::build(metrics_list, with_meta ? &metrics_meta : nullptr, entry.context().metrics());
    out["metrics"] = metrics_list;
    if (with_meta) out["metadata"] = metrics_meta;
  }
  response.append(json::serialize(out));
}

void failures_controller::clear_failures(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("failures.delete", request, response)) return;

  PB::Failures::FailuresResponseMessage message;
  if (!parse_envelope(core->clear_failures(), message)) {
    response.setCodeServerError("Failed to clear the failed-check log");
    return;
  }
  const PB::Failures::FailuresResponseMessage::Response &payload = message.payload(0);
  json::object out;
  describe(out, payload);
  out["removed"] = static_cast<std::int64_t>(payload.removed());
  response.append(json::serialize(out));
}

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
#include <nscapi/protobuf/failures.hpp>
#include <set>
#include <str/utils.hpp>
#include <str/xtos.hpp>
#include <string>
#include <utility>

#include "perf_json.hpp"

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

// The Nagios status number, the way the results endpoint reports `status`.
int status_number(const PB::Common::ResultCode code) {
  switch (code) {
    case PB::Common::ResultCode::OK:
      return 0;
    case PB::Common::ResultCode::WARNING:
      return 1;
    case PB::Common::ResultCode::CRITICAL:
      return 2;
    default:
      return 3;
  }
}

json::object to_json(const PB::Failures::FailedCheck &entry, const std::int64_t now, const bool arguments_recorded) {
  const PB::Commands::QueryResponseMessage::Response &r = entry.response();
  json::object node;
  node["index"] = static_cast<std::int64_t>(entry.index());
  node["timestamp"] = entry.timestamp();
  node["date"] = to_date(entry.timestamp());
  node["age"] = now > entry.timestamp() ? now - entry.timestamp() : 0;
  // Who asked: the module that dispatched the query and, when it knows, the
  // principal behind it (`WEBServer:admin`, `NRPEServer`, `Scheduler`).
  node["subject"] = entry.subject();
  // 0 for a check a client asked for; 1+ for one a check ran on its behalf
  // (check_multi, an alias), which is then also listed at depth 0 with the
  // combined result.
  node["depth"] = entry.depth();
  node["command"] = r.command();
  node["alias"] = r.alias();
  node["status"] = status_number(r.result());
  node["result"] = nscapi::failures::status_name(r.result());
  // The first line's text on its own, so a table does not have to dig into
  // `lines` for the one thing it shows per row.
  node["message"] = r.lines_size() > 0 ? r.lines(0).message() : "";
  json::array lines;
  for (const PB::Commands::QueryResponseMessage::Response::Line &l : r.lines()) {
    json::object line;
    line["message"] = l.message();
    line["perf"] = perf_json::to_json(l);
    lines.push_back(line);
  }
  node["lines"] = lines;
  // Only when the operator turned `record arguments` on: an entry recorded
  // while it was off carries none, and a client should be able to tell "none
  // were given" (an empty list) from "not recorded" (no key).
  if (arguments_recorded) {
    json::array arguments;
    for (const std::string &a : r.arguments()) arguments.emplace_back(a);
    node["arguments"] = arguments;
  }
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
      statuses.insert(status_number(status));
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
      if (!statuses.empty() && statuses.find(status_number(entry.response().result())) == statuses.end()) continue;
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
  }
  out["count"] = static_cast<std::int64_t>(entries.size());
  out["entries"] = entries;
  response.setHeader("X-Result-Count", str::xtos(entries.size()));
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

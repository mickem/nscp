// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "results_controller.hpp"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/date_time/c_local_time_adjustor.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/json.hpp>
#include <list>
#include <str/utils.hpp>
#include <str/xtos.hpp>
#include <string>

namespace json = boost::json;

namespace {
// Local time, matching the `date` field the log and event endpoints emit.
// The machine-readable form travels alongside it as an epoch integer, so a
// client never has to parse this.
std::string to_date(const std::int64_t timestamp) {
  if (timestamp <= 0) return "";
  const boost::posix_time::ptime utc = boost::posix_time::from_time_t(static_cast<std::time_t>(timestamp));
  return boost::posix_time::to_simple_string(boost::date_time::c_local_adjustor<boost::posix_time::ptime>::utc_to_local(utc));
}

std::string status_name(const int status) {
  switch (status) {
    case 0:
      return "OK";
    case 1:
      return "WARNING";
    case 2:
      return "CRITICAL";
    default:
      return "UNKNOWN";
  }
}

// Accepts both the words and the Nagios numbers, so `?status=critical` and
// `?status=2` are the same query. Returns false on anything else rather than
// silently returning every result.
bool parse_status(const std::string &token, int &status) {
  std::string value = token;
  boost::algorithm::trim(value);
  boost::algorithm::to_lower(value);
  if (value == "ok" || value == "0")
    status = 0;
  else if (value == "warning" || value == "warn" || value == "1")
    status = 1;
  else if (value == "critical" || value == "crit" || value == "2")
    status = 2;
  else if (value == "unknown" || value == "3")
    status = 3;
  else
    return false;
  return true;
}

json::object to_json(const result_store::result_entry &e, const std::int64_t now, const std::string &base) {
  json::object node;
  node["key"] = e.key;
  node["index"] = static_cast<std::int64_t>(e.index);
  node["channel"] = e.channel;
  node["host"] = e.host;
  node["source"] = e.source;
  node["command"] = e.command;
  node["alias"] = e.alias;
  node["status"] = e.status;
  node["result"] = status_name(e.status);
  node["message"] = e.message;
  node["perf"] = e.perf;
  node["count"] = static_cast<std::int64_t>(e.count);
  node["first_seen"] = e.first_seen;
  node["last_seen"] = e.last_seen;
  node["result_seen"] = e.result_seen;
  node["first_seen_date"] = to_date(e.first_seen);
  node["last_seen_date"] = to_date(e.last_seen);
  node["result_seen_date"] = to_date(e.result_seen);
  // How stale the result is. The whole point of a passive cache is that a
  // consumer can tell "OK" from "OK, reported three days ago".
  node["age"] = now > e.last_seen ? now - e.last_seen : 0;
  if (!base.empty()) {
    // The key is operator-defined text and can hold a space or a `%`; the URL
    // it is advertised under has to be one a client can actually fetch.
    node["result_url"] = base + "/" + result_key_encode(e.key);
  }
  return node;
}
}  // namespace

results_controller::results_controller(const int version, const std::shared_ptr<session_manager_interface> &session,
                                       const std::shared_ptr<result_store> &results)
    : RegexpController(version == 1 ? "/api/v1/results" : "/api/v2/results"), session(session), results(results) {
  addRoute("GET", "/?$", this, &results_controller::list_results);
  addRoute("DELETE", "/?$", this, &results_controller::clear_results);
  // Keys contain the primary-index separator (a `/` by default), so the
  // capture deliberately spans path segments instead of stopping at the
  // first slash the way /queries/{name} does.
  addRoute("GET", "/(.+?)/?$", this, &results_controller::get_result);
  addRoute("DELETE", "/(.+?)/?$", this, &results_controller::delete_result);
}

bool results_controller::require_enabled(Mongoose::StreamResponse &response) const {
  if (results->is_enabled()) return true;
  // Deliberately not 404 (which reads as "wrong URL / too old a version") and
  // not an empty list (which reads as "nothing has reported yet"): an
  // operator polling a switched-off cache should be told exactly that, and
  // which settings turn it on. `enabled` is not the only way to end up here -
  // an empty `channel`, or either of them changed by a settings reload
  // without a restart, leaves the store off as well - so name both and say
  // that they are read at startup.
  response.setCode(HTTP_SERVICE_UNAVAILABLE, REASON_SERVICE_UNAVAILABLE);
  response.append(
      "Passive result cache is disabled. Under /settings/WEB/server/results set enabled=true and channel to a non-empty submission channel, then "
      "restart the service: both are read when the web server starts, not on a settings reload.");
  return false;
}

void results_controller::list_results(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("results.list", request, response)) return;
  if (!require_enabled(response)) return;

  result_store::filter f;
  f.channel = request.get("channel", "");
  f.host = request.get("host", "");
  f.command = request.get("command", "");
  f.alias = request.get("alias", "");

  const std::string statuses = request.get("status", "");
  if (!statuses.empty()) {
    for (const std::string &token : str::utils::split<std::list<std::string> >(statuses, ",")) {
      int status = 0;
      if (!parse_status(token, status)) {
        response.setCodeBadRequest("Invalid status filter: " + token);
        return;
      }
      f.statuses.push_back(status);
    }
  }

  const std::int64_t now = result_store_now();
  const std::string base = get_base(request);
  // A poll consumes what it reports: the next poll then answers "what has
  // happened since you last asked" rather than repeating the same results,
  // which is what makes the `worst` cache mode mean "worst since last poll".
  const bool drained = results->clear_on_poll();
  const result_store::result_list entries = drained ? results->drain(f, now) : results->list(f, now);
  json::array root;
  for (const result_store::result_entry &e : entries) {
    root.push_back(to_json(e, now, base));
  }
  response.setHeader("X-Result-Count", str::xtos(root.size()));
  response.setHeader("X-Result-Drained", drained ? "true" : "false");
  response.append(json::serialize(root));
}

void results_controller::get_result(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("results.get", request, response)) return;
  if (!require_enabled(response)) return;
  if (!validate_arguments(1, what, response)) return;

  const std::string key = result_key_decode(what.str(1));
  const std::int64_t now = result_store_now();
  result_store::result_entry entry;
  if (!results->get(key, entry, now)) {
    response.setCodeNotFound("Result not found: " + key);
    return;
  }
  response.append(json::serialize(to_json(entry, now, get_base(request))));
}

void results_controller::clear_results(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("results.delete", request, response)) return;
  if (!require_enabled(response)) return;

  json::object node;
  node["removed"] = static_cast<std::int64_t>(results->clear());
  response.append(json::serialize(node));
}

void results_controller::delete_result(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("results.delete", request, response)) return;
  if (!require_enabled(response)) return;
  if (!validate_arguments(1, what, response)) return;

  const std::string key = result_key_decode(what.str(1));
  if (!results->remove(key, result_store_now())) {
    response.setCodeNotFound("Result not found: " + key);
    return;
  }
  json::object node;
  node["removed"] = 1;
  response.append(json::serialize(node));
}

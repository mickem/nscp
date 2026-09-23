// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "facts_controller.hpp"

#include <boost/json.hpp>
#include <string>
#include <utility>

namespace json = boost::json;

facts_controller::facts_controller(const int version, std::shared_ptr<session_manager_interface> session, const nscapi::core_wrapper *core,
                                   unsigned int plugin_id)
    : RegexpController(version == 1 ? "/api/v1/facts" : "/api/v2/facts"), session(std::move(session)), core(core), plugin_id(plugin_id) {
  // The refresh route is registered first: `/refresh` would otherwise be read
  // as a fact path by the route below it.
  addRoute("POST", "/refresh/?$", this, &facts_controller::refresh_facts);
  addRoute("GET", "/?$", this, &facts_controller::get_facts);
  addRoute("GET", "/(.+?)/?$", this, &facts_controller::get_fact_path);
}

bool facts_controller::is_safe_fact_path(const std::string &path) {
  if (path.empty() || path.size() > 256) return false;
  bool at_start = true;
  for (const char c : path) {
    if (c == '.') {
      // A leading, trailing or doubled dot is an empty component, which names
      // nothing the repository holds.
      if (at_start) return false;
      at_start = true;
      continue;
    }
    const bool first_ok = c >= 'a' && c <= 'z';
    const bool rest_ok = first_ok || (c >= '0' && c <= '9') || c == '_';
    if (at_start ? !first_ok : !rest_ok) return false;
    at_start = false;
  }
  return !at_start;
}

namespace {
// The core answers a facts query with the whole envelope already serialised,
// so the common case is a pass-through. Only the ETag has to be read out of
// it, and only the path routes have to look at `found`.
const json::value *field(const json::value &document, const char *key) {
  if (!document.is_object()) return nullptr;
  return document.get_object().if_contains(key);
}

// The document hash doubles as the ETag: the UI polls this endpoint and the
// document changes far more slowly than it is read.
void tag_response(Mongoose::StreamResponse &response, const json::value &document) {
  const json::value *hash = field(document, "hash");
  if (hash != nullptr && hash->is_string()) {
    response.get_headers()["ETag"] = "\"" + std::string(hash->get_string()) + "\"";
  }
}
}  // namespace

void facts_controller::get_facts(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("facts.get", request, response)) return;

  const std::string body = core->get_facts_json();
  try {
    tag_response(response, json::parse(body));
  } catch (const std::exception &) {
    // An unparsable envelope is the core's problem to report, not a reason to
    // withhold what it said; the body still goes out, just without an ETag.
  }
  response.get_headers()["Content-Type"] = "application/json";
  response.append(body);
}

void facts_controller::get_fact_path(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("facts.get", request, response)) return;

  if (what.size() != 2) {
    response.setCodeNotFound("Fact not found");
    return;
  }
  const std::string path = what.str(1);
  if (!is_safe_fact_path(path)) {
    response.setCodeBadRequest("Invalid fact path: expected snake_case components separated by dots, e.g. software.installed");
    return;
  }

  const std::string body = core->get_facts_json(path);
  json::value document;
  try {
    document = json::parse(body);
  } catch (const std::exception &) {
    response.setCodeServerError("Failed to read facts");
    return;
  }
  const json::value *found = field(document, "found");
  if (found == nullptr || !found->is_bool() || !found->get_bool()) {
    // Absent rather than empty: a path nobody produced is a 404, so a caller
    // can tell "this host has no docker facts" from "docker facts are empty".
    response.setCodeNotFound("No facts at: " + path);
    return;
  }
  tag_response(response, document);
  response.get_headers()["Content-Type"] = "application/json";
  response.append(body);
}

void facts_controller::refresh_facts(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("facts.refresh", request, response)) return;

  const std::string body = core->refresh_facts();
  try {
    tag_response(response, json::parse(body));
  } catch (const std::exception &) {
    // As above: an unparsable envelope costs the ETag, not the answer.
  }
  response.get_headers()["Content-Type"] = "application/json";
  response.append(body);
}

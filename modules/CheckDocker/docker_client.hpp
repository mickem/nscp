// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Helpers shared by the CheckDocker commands: tolerant JSON access, the
// stable error contract, and the fetch-and-parse step.

#include <boost/json.hpp>
#include <cctype>
#include <list>
#include <nscapi/nscapi_program_options.hpp>
#include <parsers/where/helpers.hpp>
#include <str/format.hpp>
#include <str/xtos.hpp>
#include <string>

#include "check_docker.hpp"

namespace docker_checks {

// The daemon answered, but with a non-2xx HTTP status. Distinct from a
// transport failure (which the fetcher reports as some other exception): a 404
// here means the specific resource is gone - most usefully, a container removed
// between the list call and its own inspect - which is a "skip it" rather than
// a "the daemon is unreachable".
class docker_http_error : public std::runtime_error {
  long status_;

 public:
  docker_http_error(const long status, const std::string &message) : std::runtime_error(message), status_(status) {}
  long status() const { return status_; }
};

// Unversioned paths: the daemon serves them with the newest API it speaks,
// which every docker release (and podman's compat API) accepts. A pinned
// version breaks in both directions - the previous /v1.40 predates what
// current daemons still serve ("client version 1.40 is too old. Minimum
// supported API version is 1.41") - and everything these checks read has been
// present since long before any supported daemon.
const char *const API = "";

// --- tolerant JSON accessors -------------------------------------------------
//
// The daemon's payload varies by version and container configuration (podman's
// compat API differs in places too); a missing or differently-typed field must
// degrade to an empty value, never throw.

inline std::string get_str(const boost::json::object &o, const char *key) {
  if (const boost::json::value *p = o.if_contains(key)) {
    if (p->is_string()) return std::string(p->as_string().c_str());
  }
  return "";
}

inline long long get_num(const boost::json::object &o, const char *key) {
  if (const boost::json::value *p = o.if_contains(key)) {
    if (p->is_int64()) return p->as_int64();
    if (p->is_uint64()) return static_cast<long long>(p->as_uint64());
    if (p->is_double()) return static_cast<long long>(p->as_double());
  }
  return 0;
}

inline bool get_bool(const boost::json::object &o, const char *key) {
  if (const boost::json::value *p = o.if_contains(key)) {
    if (p->is_bool()) return p->as_bool();
  }
  return false;
}

// Nested object access; nullptr when absent or not an object.
inline const boost::json::object *get_obj(const boost::json::object &o, const char *key) {
  if (const boost::json::value *p = o.if_contains(key)) {
    if (p->is_object()) return &p->as_object();
  }
  return nullptr;
}

// Docker reports names as "/name"; the slash is an API artifact.
inline std::string strip_slash(const std::string &name) { return !name.empty() && name[0] == '/' ? name.substr(1) : name; }

// set_response_bad appends, so a failure raised after post_process() has
// already written the result line would produce a garbled two-line UNKNOWN.
// Drop anything already rendered so the error is the only thing reported.
inline void fail(PB::Commands::QueryResponseMessage::Response *response, const std::string &message) {
  response->clear_lines();
  nscapi::protobuf::functions::set_response_bad(*response, message);
}

// Fetch and parse one API endpoint with the module's stable error contract:
// transport failures become "Failed to connect to docker daemon at ...", bad
// payloads "Failed to parse ..." - both UNKNOWN. Returns false when the
// response has already been failed.
inline bool fetch_json(const fetcher &fetch, const std::string &endpoint, const std::string &path, boost::json::value &out,
                       PB::Commands::QueryResponseMessage::Response *response) {
  std::string body;
  try {
    body = fetch(path);
  } catch (const docker_http_error &e) {
    // A non-2xx on a top-level call (the container list, /info, /system/df) is a
    // real failure - it connected, but the daemon would not serve the request.
    fail(response, "docker daemon at '" + endpoint + "' returned HTTP " + std::to_string(e.status()) + " for " + path + ": " + e.what());
    return false;
  } catch (const std::exception &e) {
    fail(response, "Failed to connect to docker daemon at '" + endpoint + "': " + utf8::utf8_from_native(e.what()));
    return false;
  }
  try {
    out = boost::json::parse(body);
  } catch (const std::exception &e) {
    fail(response, "Failed to parse docker daemon response from " + path + ": " + utf8::utf8_from_native(e.what()));
    return false;
  }
  return true;
}

// What happened to a per-item fetch (a single container's inspect or stats),
// which - unlike the list call above - races against the container's lifetime.
enum class item_fetch {
  ok,        // parsed into `out`
  vanished,  // the item is gone (HTTP 404); skip it and carry on
  failed,    // the daemon is unreachable or misbehaving; the check is aborted
};

// Fetch one per-item resource. A 404 (the container was removed between the
// list call and this one - routine on a host running `docker run --rm` jobs)
// is `vanished`, so the caller drops that container and keeps the rest instead
// of throwing the whole check away and blaming the socket. Any other error
// fails the response, exactly as fetch_json would.
inline item_fetch fetch_json_item(const fetcher &fetch, const std::string &endpoint, const std::string &path, boost::json::value &out,
                                  PB::Commands::QueryResponseMessage::Response *response) {
  std::string body;
  try {
    body = fetch(path);
  } catch (const docker_http_error &e) {
    if (e.status() == 404) return item_fetch::vanished;
    fail(response, "docker daemon at '" + endpoint + "' returned HTTP " + std::to_string(e.status()) + " for " + path + ": " + e.what());
    return item_fetch::failed;
  } catch (const std::exception &e) {
    fail(response, "Failed to connect to docker daemon at '" + endpoint + "': " + utf8::utf8_from_native(e.what()));
    return item_fetch::failed;
  }
  try {
    out = boost::json::parse(body);
  } catch (const std::exception &e) {
    fail(response, "Failed to parse docker daemon response from " + path + ": " + utf8::utf8_from_native(e.what()));
    return item_fetch::failed;
  }
  return item_fetch::ok;
}

// --- duration-literal converter ----------------------------------------------

// True for an optionally-signed run of digits ("0", "-1", "+259200").
inline bool is_plain_integer(const std::string &expr) {
  std::size_t i = 0;
  if (i < expr.size() && (expr[i] == '-' || expr[i] == '+')) ++i;
  if (i >= expr.size()) return false;
  for (; i < expr.size(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(expr[i]))) return false;
  }
  return true;
}

// Duration-literal converter for age-style keywords (register with
// add_converter on a type_custom_int_* keyword): turns "30m" / "2d" - or the
// tokenized [number, unit] list form - into seconds, so expressions like
// started < 10m work. Same shape as mssql_filter::parse_time in CheckMSSQL;
// plain integers pass straight through so -1 sentinels keep working.
template <class TObject>
parsers::where::node_type parse_time(TObject object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
  using namespace parsers::where;
  std::list<node_type> tokens = subject->get_list_value(context);
  std::string expr;
  if (tokens.size() == 2) {
    auto cit = tokens.begin();
    const long long n = (*cit)->get_int_value(context);
    ++cit;
    const std::string unit = (*cit)->get_value(context, type_string).get_string("");
    expr = str::xtos(n) + unit;
  } else {
    expr = subject->get_string_value(context);
  }
  if (is_plain_integer(expr)) return factory::create_int(str::stox<long long>(expr, 0));
  return factory::create_int(str::format::stox_as_time_sec<long long>(expr, "s"));
}

}  // namespace docker_checks

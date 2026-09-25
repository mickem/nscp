// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Helpers shared by the CheckKubernetes commands: the fetcher contract,
// tolerant JSON access, the stable UNKNOWN error contract, paginated list
// calls and the duration-literal converter for age keywords.

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/json.hpp>
#include <cctype>
#include <cstdio>
#include <functional>
#include <list>
#include <nscapi/nscapi_program_options.hpp>
#include <parsers/where/helpers.hpp>
#include <str/format.hpp>
#include <str/utf8.hpp>
#include <str/xtos.hpp>
#include <string>
#include <vector>

#include "kube_json.hpp"
#include "kube_settings.hpp"

namespace kube_checks {

// The API server answered with a non-2xx status. Distinct from a transport
// failure (any other exception out of the fetcher): here the server was
// reached, and the status says why it would not serve the request.
class kube_http_error : public std::runtime_error {
  long status_;
  std::string body_;

 public:
  kube_http_error(const long status, const std::string &message, std::string body = std::string())
      : std::runtime_error(message), status_(status), body_(std::move(body)) {}
  long status() const { return status_; }
  const std::string &body() const { return body_; }
};

// GET `path` (an absolute API path with its query string) from the API server
// and return the response body. Throws kube_http_error on a non-2xx status and
// some other exception on a transport failure.
typedef std::function<std::string(const std::string &path)> fetcher;

// Creates a fetcher for a resolved cluster. Injectable so the checks (and
// their unit tests) never touch the HTTP client directly; the module wires in
// the real HTTPS transport from CheckKubernetes.cpp.
typedef std::function<fetcher(const cluster &target)> fetcher_factory;

// "key=value,key=value" from a labels/annotations map.
inline std::string join_map(const boost::json::object &o, const char *key) {
  std::string out;
  if (const boost::json::object *m = get_obj(o, key)) {
    for (const auto &kv : *m) {
      if (kv.value().is_string()) str::format::append_list(out, std::string(kv.key()) + "=" + kv.value().as_string().c_str(), ",");
    }
  }
  return out;
}

// --- time ---------------------------------------------------------------------

// Seconds since an RFC3339 timestamp ("2026-08-12T07:44:00Z"); -1 when absent
// or unparsable.
inline long long seconds_since(const std::string &rfc3339) {
  if (rfc3339.empty() || rfc3339[0] == '0') return -1;
  std::string s = rfc3339;
  const auto dot = s.find('.');
  if (dot != std::string::npos) {
    s = s.substr(0, dot);
  } else if (!s.empty() && s.back() == 'Z') {
    s.pop_back();
  }
  try {
    const boost::posix_time::ptime t = boost::posix_time::from_iso_extended_string(s);
    const boost::posix_time::ptime now = boost::posix_time::second_clock::universal_time();
    return (now - t).total_seconds();
  } catch (const std::exception &) {
    return -1;
  }
}

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
// tokenized [number, unit] list form - into seconds, so `age < 10m` works.
// Plain integers pass straight through so -1 sentinels keep working.
template <class TObject>
parsers::where::node_type parse_time(TObject, parsers::where::evaluation_context context, parsers::where::node_type subject) {
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

// --- the error contract -------------------------------------------------------

// set_response_bad appends, so a failure raised after post_process() has
// already written the result line would produce a garbled two-line UNKNOWN.
// Drop anything already rendered so the error is the only thing reported.
inline void fail(PB::Commands::QueryResponseMessage::Response *response, const std::string &message) {
  response->clear_lines();
  nscapi::protobuf::functions::set_response_bad(*response, message);
}

// The `message` of a Kubernetes Status body ("pods is forbidden: User ... cannot
// list resource \"pods\" ..."); empty when the body is not one.
inline std::string status_message(const std::string &body) {
  if (body.empty()) return "";
  try {
    const boost::json::value v = boost::json::parse(body);
    if (v.is_object()) return get_str(v.as_object(), "message");
  } catch (const std::exception &) {
  }
  return "";
}

// One line for a non-2xx answer, actionable and without the credential:
// 401 is the token, 403 is RBAC, 404 under metrics.k8s.io is metrics-server.
inline std::string describe_http_error(const cluster &target, const std::string &path, const kube_http_error &e) {
  const std::string where = "Kubernetes API server at '" + target.address() + "'";
  const std::string detail = status_message(e.body());
  const std::string http = "HTTP " + std::to_string(e.status());
  const std::string request = "GET " + path;
  if (e.status() == 401) {
    return where + " rejected the credentials (" + http + " for " + request +
           "): the bearer token is invalid or expired - check `token` / `token file` under [/settings/kubernetes]";
  }
  if (e.status() == 403) {
    return where + " denied " + request + " (" + http + (detail.empty() ? "" : ": " + detail) +
           "): grant the agent's service account get and list on the resource (see the CheckKubernetes documentation for the ClusterRole)";
  }
  if (e.status() == 404 && path.find("/apis/metrics.k8s.io/") == 0) {
    return "metrics-server is not installed in the cluster at '" + target.address() + "' (" + http + " for " + request + ")";
  }
  return where + " returned " + http + " for " + request + (detail.empty() ? ": " + std::string(e.what()) : ": " + detail);
}

inline std::string describe_transport_error(const cluster &target, const std::exception &e) {
  return "Failed to connect to Kubernetes API server at '" + target.address() + "' (" + target.source + "): " + utf8::utf8_from_native(e.what());
}

// What happened to a fetch whose non-2xx status is itself an answer (see
// fetch_raw), rather than a failure.
enum class raw_fetch {
  ok,        // 2xx; `body` holds the payload
  rejected,  // non-2xx; `status` holds the code and `body` whatever came with it
  failed,    // the server is unreachable; the response has been failed
};

// Fetch one path without treating a non-2xx status as a failure: /readyz
// answers 500 when a check fails, and that is the finding, not an error.
inline raw_fetch fetch_raw(const fetcher &fetch, const cluster &target, const std::string &path, std::string &body, long &status,
                           PB::Commands::QueryResponseMessage::Response *response) {
  try {
    body = fetch(path);
    status = 200;
    return raw_fetch::ok;
  } catch (const kube_http_error &e) {
    status = e.status();
    body = e.body();
    return raw_fetch::rejected;
  } catch (const std::exception &e) {
    fail(response, describe_transport_error(target, e));
    return raw_fetch::failed;
  }
}

// Fetch and parse one API path with the module's error contract. Returns
// false when the response has already been failed.
inline bool fetch_json(const fetcher &fetch, const cluster &target, const std::string &path, boost::json::value &out,
                       PB::Commands::QueryResponseMessage::Response *response) {
  std::string body;
  try {
    body = fetch(path);
  } catch (const kube_http_error &e) {
    fail(response, describe_http_error(target, path, e));
    return false;
  } catch (const std::exception &e) {
    fail(response, describe_transport_error(target, e));
    return false;
  }
  try {
    out = boost::json::parse(body);
  } catch (const std::exception &e) {
    fail(response, "Failed to parse the Kubernetes API server response from " + path + ": " + utf8::utf8_from_native(e.what()));
    return false;
  }
  return true;
}

// --- list calls ---------------------------------------------------------------

// Percent-encode a query parameter value (RFC 3986 unreserved characters pass).
inline std::string url_encode(const std::string &value) {
  std::string out;
  out.reserve(value.size());
  for (const unsigned char c : value) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out.push_back(static_cast<char>(c));
    } else {
      char buf[4];
      std::snprintf(buf, sizeof(buf), "%%%02X", c);
      out += buf;
    }
  }
  return out;
}

// The selectors a list check passes to the server, so filtering happens
// before the payload is built.
struct list_options {
  std::string label_selector;
  std::string field_selector;
  long limit = 500;  // page size; the server caps it anyway
};

inline std::string list_query(const list_options &opt, const std::string &continue_token) {
  std::string q;
  const auto add = [&q](const std::string &key, const std::string &value) {
    if (value.empty()) return;
    q += (q.empty() ? "?" : "&") + key + "=" + url_encode(value);
  };
  if (opt.limit > 0) add("limit", std::to_string(opt.limit));
  add("labelSelector", opt.label_selector);
  add("fieldSelector", opt.field_selector);
  add("continue", continue_token);
  return q;
}

// The list path for a resource: cluster-wide, or scoped to one namespace.
// `prefix` is the API group path ("/api/v1", "/apis/apps/v1").
inline std::string list_path(const std::string &prefix, const std::string &ns, const std::string &resource) {
  if (ns.empty()) return prefix + "/" + resource;
  return prefix + "/namespaces/" + url_encode(ns) + "/" + resource;
}

// GET a list resource, following `metadata.continue` until the server has no
// more pages, and append every item to `items`. Returns false when the
// response has been failed.
inline bool list_all(const fetcher &fetch, const cluster &target, const std::string &path, const list_options &opt, std::vector<boost::json::value> &items,
                     PB::Commands::QueryResponseMessage::Response *response) {
  std::string continue_token;
  // A server that keeps handing back the same token would loop forever;
  // no real cluster has this many pages of 500.
  for (int page = 0; page < 10000; ++page) {
    boost::json::value root;
    if (!fetch_json(fetch, target, path + list_query(opt, continue_token), root, response)) return false;
    if (!root.is_object()) {
      fail(response, "Failed to parse the Kubernetes API server response from " + path + ": expected a list object");
      return false;
    }
    const boost::json::object &o = root.as_object();
    if (const boost::json::array *list = get_arr(o, "items")) {
      for (const auto &item : *list) items.push_back(item);
    }
    std::string next;
    if (const boost::json::object *md = metadata_of(o)) next = get_str(*md, "continue");
    if (next.empty()) return true;
    continue_token = next;
  }
  fail(response, "The Kubernetes API server at '" + target.address() + "' kept paginating " + path + " past 10000 pages");
  return false;
}

// List one resource across the requested namespaces (none = all namespaces).
inline bool list_namespaced(const fetcher &fetch, const cluster &target, const std::string &prefix, const std::string &resource,
                            const std::vector<std::string> &namespaces, const list_options &opt, std::vector<boost::json::value> &items,
                            PB::Commands::QueryResponseMessage::Response *response) {
  if (namespaces.empty()) return list_all(fetch, target, list_path(prefix, "", resource), opt, items, response);
  for (const std::string &ns : namespaces) {
    if (!list_all(fetch, target, list_path(prefix, ns, resource), opt, items, response)) return false;
  }
  return true;
}

}  // namespace kube_checks

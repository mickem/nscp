// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Helpers shared by the CheckKubernetes commands: the fetcher contract, the
// stable UNKNOWN error contract, paginated list calls and the "must exist"
// bookkeeping behind pod= / node= / workload=.

#include <boost/json.hpp>
#include <cctype>
#include <cstdio>
#include <functional>
#include <nscapi/nscapi_program_options.hpp>
#include <str/format.hpp>
#include <str/utf8.hpp>
#include <string>
#include <utility>
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
// the real HTTPS transport from CheckKubernetes.cpp. A check calls it once
// and issues every request of that invocation through the one fetcher.
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
// 401 is the token, 403 is the RBAC rule to grant.
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
    // A resource path wants get/list on the resource; /version, /readyz and
    // friends are non-resource URLs and want a nonResourceURLs rule.
    const bool resource = path.compare(0, 4, "/api") == 0;
    const std::string bare = path.substr(0, path.find('?'));
    const std::string hint = resource ? "grant the agent's service account get and list on the resource"
                                      : "grant the agent's service account get on the non-resource URL " + bare + " (a nonResourceURLs rule)";
    return where + " denied " + request + " (" + http + (detail.empty() ? "" : ": " + detail) + "): " + hint +
           " (see the CheckKubernetes documentation for the ClusterRole)";
  }
  return where + " returned " + http + " for " + request + (detail.empty() ? ": " + std::string(e.what()) : ": " + detail);
}

// True for the statuses describe_http_error turns into "fix your credentials
// or RBAC": those are UNKNOWN on every path, never a finding about the
// cluster.
inline bool is_auth_error(const long status) { return status == 401 || status == 403; }

inline std::string describe_transport_error(const cluster &target, const std::exception &e) {
  return "Failed to connect to Kubernetes API server at '" + target.address() + "' (" + target.source + "): " + utf8::utf8_from_native(e.what());
}

// Build the check's fetcher. The real factory constructs the TLS client,
// which loads the CA bundle and parses the client certificate up front, so a
// missing `ca` file or non-PEM identity data throws here rather than on the
// first request. Report it the way a failed connection is reported instead of
// letting it escape the check as a bare command failure. False when the
// response has been failed.
inline bool open_fetcher(const fetcher_factory &make_fetcher, const cluster &target, fetcher &out, PB::Commands::QueryResponseMessage::Response *response) {
  try {
    out = make_fetcher(target);
    return true;
  } catch (const std::exception &e) {
    fail(response, describe_transport_error(target, e));
    return false;
  }
}

// What happened to a fetch whose non-2xx status is itself an answer (see
// fetch_raw), rather than a failure.
enum class raw_fetch {
  ok,        // 2xx; `body` holds the payload
  rejected,  // non-2xx; `status` holds the code and `body` whatever came with it
  failed,    // the server is unreachable, or refused the credentials; the response has been failed
};

// Fetch one path without treating a non-2xx status as a failure: /readyz
// answers 500 when a check fails, and that is the finding, not an error. A
// 401 or 403 is still an error - the server never got to answer the
// question - and is reported under the usual contract.
inline raw_fetch fetch_raw(const fetcher &fetch, const cluster &target, const std::string &path, std::string &body, long &status,
                           PB::Commands::QueryResponseMessage::Response *response) {
  try {
    body = fetch(path);
    status = 200;
    return raw_fetch::ok;
  } catch (const kube_http_error &e) {
    if (is_auth_error(e.status())) {
      fail(response, describe_http_error(target, path, e));
      return raw_fetch::failed;
    }
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
    boost::json::object &o = root.as_object();
    // Moved, not copied: the page is dropped right after, and a pod list in
    // a large cluster is the biggest thing this module ever holds.
    if (boost::json::value *list = o.if_contains("items")) {
      if (list->is_array()) {
        for (auto &item : list->as_array()) items.push_back(std::move(item));
      }
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

// --- "must exist" names -------------------------------------------------------

// The bookkeeping behind pod= / node= / workload=: only the named objects
// take part in the check, and one the server did not return is synthesised
// by the caller so it shows up (and trips the default critical) instead of
// silently disappearing from the listing. A name is given bare or as
// namespace/name.
class required_names {
  std::vector<std::string> names_;
  std::vector<bool> seen_;

 public:
  explicit required_names(std::vector<std::string> names) : names_(std::move(names)), seen_(names_.size(), false) {}

  bool empty() const { return names_.empty(); }

  // True when the object is one of the required names, by its bare name or
  // by `qualified` (namespace/name); marks every matching entry as seen.
  bool claim(const std::string &name, const std::string &qualified = std::string()) {
    bool matched = false;
    for (std::size_t i = 0; i < names_.size(); ++i) {
      if (names_[i] == name || (!qualified.empty() && names_[i] == qualified)) {
        seen_[i] = true;
        matched = true;
      }
    }
    return matched;
  }

  // Calls fn(namespace, name) for every required name the server did not
  // return. A bare name gets `default_ns`: the one namespace the check was
  // scoped to, or "*" when it looked everywhere.
  template <class F>
  void for_each_missing(const std::string &default_ns, F fn) const {
    for (std::size_t i = 0; i < names_.size(); ++i) {
      if (seen_[i]) continue;
      const auto slash = names_[i].find('/');
      if (slash != std::string::npos) {
        fn(names_[i].substr(0, slash), names_[i].substr(slash + 1));
      } else {
        fn(default_ns, names_[i]);
      }
    }
  }
};

// The namespace a bare required name is reported under: the single
// namespace= the check was scoped to, else "*" for "any".
inline std::string default_namespace(const std::vector<std::string> &namespaces) { return namespaces.size() == 1 ? namespaces[0] : "*"; }

}  // namespace kube_checks

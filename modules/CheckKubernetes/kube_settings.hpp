// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Cluster configuration for the CheckKubernetes commands: what the operator
// wrote under [/settings/kubernetes], and how it resolves into one API server
// connection (explicit settings, then a JSON kubeconfig, then the in-cluster
// service account environment).
//
// The cluster is an operator decision. A check request selects nothing here:
// no `url=`, `host=` or `token=` argument exists, because a caller who can run
// a check (over REST, anyone holding `queries.execute`) must not be able to
// point the agent - and its bearer token - at a server of their choosing.

#include <bytes/base64.h>

#include <boost/algorithm/string/trim.hpp>
#include <boost/filesystem.hpp>
#include <boost/json.hpp>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace kube_checks {

// Module-level settings, straight from [/settings/kubernetes].
struct settings {
  std::string api_server;  // https://host:6443; empty = in-cluster auto-detect
  std::string token;       // bearer token (redacted in listings: registered with add_password)
  std::string token_file;  // path to a token file, read at check time
  std::string kubeconfig;  // path to a kubeconfig in JSON form
  std::string context;     // kubeconfig context to use; empty = current-context
  std::string ca;          // CA bundle (file or hashed directory); the ${ca-path} default is expanded by the settings layer
  std::string verify_mode = "peer";
  std::string tls_version = "tlsv1.2+";
  int timeout = 30;          // per-request deadline, seconds
  int max_response_mb = 64;  // fetch() buffer cap: a pod list in a large cluster is well past the client's 5 MB default
};

// One resolved API server connection: everything the fetcher needs and nothing
// a check can change.
struct cluster {
  std::string protocol = "https";
  std::string host;
  std::string port = "443";
  std::string base_path;  // path prefix of the server url, for API servers behind a path-routing proxy
  std::string token;
  std::string ca;               // CA bundle path; empty = the TLS stack's default (nothing) for `verify mode = none`
  std::string ca_pem;           // CA in memory (a kubeconfig's certificate-authority-data)
  std::string client_cert_pem;  // mutual TLS from a kubeconfig's client-certificate-data / client-key-data
  std::string client_key_pem;
  std::string verify_mode = "peer";
  std::string tls_version = "tlsv1.2+";
  int timeout = 30;
  std::size_t max_response_bytes = 64u * 1024u * 1024u;
  std::string source;  // where the configuration came from, for messages: "settings", "kubeconfig <path>", "in-cluster"

  // The address as it appears in messages: never the token.
  std::string address() const { return protocol + "://" + host_header() + (base_path.empty() ? "" : base_path); }

  // host[:port] for the Host header; an IPv6 literal is bracketed.
  std::string host_header() const {
    const std::string h = host.find(':') != std::string::npos ? "[" + host + "]" : host;
    const bool default_port = (protocol == "https" && port == "443") || (protocol == "http" && port == "80");
    return default_port ? h : h + ":" + port;
  }
};

namespace detail {

inline bool read_file(const std::string &path, std::string &out) {
  std::ifstream in(path.c_str(), std::ios::in | std::ios::binary);
  if (!in) return false;
  std::ostringstream ss;
  ss << in.rdbuf();
  out = ss.str();
  return true;
}

// A token file holds the token and, typically, a trailing newline.
inline bool read_token_file(const std::string &path, std::string &token, std::string &error) {
  std::string data;
  if (!read_file(path, data)) {
    error = "Failed to read token file '" + path + "'";
    return false;
  }
  boost::algorithm::trim(data);
  if (data.empty()) {
    error = "Token file '" + path + "' is empty";
    return false;
  }
  token = data;
  return true;
}

inline std::string base64_decode(const std::string &encoded) {
  // Kubeconfig data fields are one line, but a hand-edited one may wrap.
  std::string compact;
  compact.reserve(encoded.size());
  for (const char c : encoded) {
    if (c != '\n' && c != '\r' && c != ' ' && c != '\t') compact.push_back(c);
  }
  if (compact.empty()) return "";
  const std::size_t needed = b64::b64_decode(compact.c_str(), compact.size(), nullptr, 0);
  if (needed == 0) return "";
  std::string out(needed, '\0');
  const std::size_t written = b64::b64_decode(compact.c_str(), compact.size(), &out[0], needed);
  if (written == 0) return "";
  out.resize(written);
  return out;
}

// A file named by a kubeconfig is relative to the kubeconfig's own
// directory, not to the agent's working directory (client-go resolves it the
// same way, so a kubeconfig that works with kubectl works here).
inline std::string resolve_relative(const std::string &base_dir, const std::string &path) {
  if (path.empty() || base_dir.empty()) return path;
  const boost::filesystem::path p(path);
  if (p.is_absolute()) return path;
  return (boost::filesystem::path(base_dir) / p).string();
}

inline std::string json_str(const boost::json::object &o, const char *key) {
  if (const boost::json::value *p = o.if_contains(key)) {
    if (p->is_string()) return std::string(p->as_string().c_str());
  }
  return "";
}

inline bool json_bool(const boost::json::object &o, const char *key) {
  if (const boost::json::value *p = o.if_contains(key)) {
    if (p->is_bool()) return p->as_bool();
  }
  return false;
}

// The named entry of a kubeconfig list ("clusters", "contexts", "users"): the
// object under `section` of the item whose "name" matches. nullptr when absent.
inline const boost::json::object *named_entry(const boost::json::object &root, const char *list, const std::string &name, const char *section) {
  const boost::json::value *l = root.if_contains(list);
  if (!l || !l->is_array()) return nullptr;
  for (const auto &item : l->as_array()) {
    if (!item.is_object()) continue;
    const boost::json::object &o = item.as_object();
    if (json_str(o, "name") != name) continue;
    if (const boost::json::value *s = o.if_contains(section)) {
      if (s->is_object()) return &s->as_object();
    }
    return nullptr;
  }
  return nullptr;
}

}  // namespace detail

// Split "https://host:6443/prefix" into the cluster's protocol, host, port and
// base path. Accepts a bare "host:6443" as https.
inline bool parse_server_url(const std::string &url_in, cluster &out, std::string &error) {
  std::string url = url_in;
  boost::algorithm::trim(url);
  if (url.empty()) {
    error = "API server url is empty";
    return false;
  }
  std::string rest = url;
  const auto scheme_end = url.find("://");
  if (scheme_end != std::string::npos) {
    out.protocol = url.substr(0, scheme_end);
    rest = url.substr(scheme_end + 3);
    if (out.protocol != "https" && out.protocol != "http") {
      error = "Unsupported API server url '" + url + "': expected https:// (or http://)";
      return false;
    }
  } else {
    out.protocol = "https";
  }
  out.port = out.protocol == "http" ? "80" : "443";
  const auto path_start = rest.find('/');
  std::string authority = rest;
  if (path_start != std::string::npos) {
    authority = rest.substr(0, path_start);
    std::string path = rest.substr(path_start);
    while (!path.empty() && path.back() == '/') path.pop_back();
    out.base_path = path;
  }
  if (authority.empty()) {
    error = "API server url '" + url + "' has no host";
    return false;
  }
  if (authority[0] == '[') {
    // [v6::literal]:port
    const auto close = authority.find(']');
    if (close == std::string::npos) {
      error = "API server url '" + url + "' has an unterminated IPv6 literal";
      return false;
    }
    out.host = authority.substr(1, close - 1);
    if (close + 1 < authority.size()) {
      if (authority[close + 1] != ':' || close + 2 >= authority.size()) {
        error = "API server url '" + url + "' has an invalid port";
        return false;
      }
      out.port = authority.substr(close + 2);
    }
  } else {
    const auto colon = authority.rfind(':');
    if (colon != std::string::npos && authority.find(':') == colon) {
      out.host = authority.substr(0, colon);
      out.port = authority.substr(colon + 1);
    } else {
      out.host = authority;
    }
  }
  if (out.host.empty()) {
    error = "API server url '" + url + "' has no host";
    return false;
  }
  for (const char c : out.port) {
    if (c < '0' || c > '9') {
      error = "API server url '" + url + "' has an invalid port '" + out.port + "'";
      return false;
    }
  }
  if (out.port.empty()) {
    error = "API server url '" + url + "' has an empty port";
    return false;
  }
  return true;
}

// Resolve a kubeconfig in JSON form (`kubectl config view --raw --minify -o
// json`). YAML is not accepted: the agent carries no YAML parser. Files the
// kubeconfig names by a relative path are looked up under `base_dir`, the
// directory the kubeconfig itself lives in.
inline bool resolve_kubeconfig(const std::string &text, const std::string &base_dir, const std::string &context_name, const std::string &kubeconfig_label,
                               cluster &out, std::string &error) {
  boost::json::value root;
  try {
    root = boost::json::parse(text);
  } catch (const std::exception &e) {
    error = "Failed to parse kubeconfig " + kubeconfig_label +
            " as JSON (YAML is not supported, convert it with `kubectl config view --raw --minify -o json`): " + e.what();
    return false;
  }
  if (!root.is_object()) {
    error = "Failed to parse kubeconfig " + kubeconfig_label + ": expected a JSON object";
    return false;
  }
  const boost::json::object &cfg = root.as_object();
  const std::string ctx_name = context_name.empty() ? detail::json_str(cfg, "current-context") : context_name;
  if (ctx_name.empty()) {
    error = "Kubeconfig " + kubeconfig_label + " has no current-context; set `context` under [/settings/kubernetes]";
    return false;
  }
  const boost::json::object *ctx = detail::named_entry(cfg, "contexts", ctx_name, "context");
  if (!ctx) {
    error = "Kubeconfig " + kubeconfig_label + " has no context named '" + ctx_name + "'";
    return false;
  }
  const std::string cluster_name = detail::json_str(*ctx, "cluster");
  const std::string user_name = detail::json_str(*ctx, "user");
  const boost::json::object *cl = detail::named_entry(cfg, "clusters", cluster_name, "cluster");
  if (!cl) {
    error = "Kubeconfig " + kubeconfig_label + ": context '" + ctx_name + "' names a cluster '" + cluster_name + "' that does not exist";
    return false;
  }
  if (!parse_server_url(detail::json_str(*cl, "server"), out, error)) {
    error = "Kubeconfig " + kubeconfig_label + ", cluster '" + cluster_name + "': " + error;
    return false;
  }
  const std::string ca_data = detail::json_str(*cl, "certificate-authority-data");
  if (!ca_data.empty()) {
    out.ca_pem = detail::base64_decode(ca_data);
    if (out.ca_pem.empty()) {
      error = "Kubeconfig " + kubeconfig_label + ", cluster '" + cluster_name + "': certificate-authority-data is not valid base64";
      return false;
    }
  } else {
    const std::string ca_file = detail::json_str(*cl, "certificate-authority");
    if (!ca_file.empty()) out.ca = detail::resolve_relative(base_dir, ca_file);
  }
  const bool insecure = detail::json_bool(*cl, "insecure-skip-tls-verify");
  if (insecure) out.verify_mode = "none";

  const boost::json::object *user = detail::named_entry(cfg, "users", user_name, "user");
  if (!user) {
    error = "Kubeconfig " + kubeconfig_label + ": context '" + ctx_name + "' names a user '" + user_name + "' that does not exist";
    return false;
  }
  out.token = detail::json_str(*user, "token");
  if (out.token.empty()) {
    const std::string token_file = detail::resolve_relative(base_dir, detail::json_str(*user, "tokenFile"));
    if (!token_file.empty() && !detail::read_token_file(token_file, out.token, error)) {
      error = "Kubeconfig " + kubeconfig_label + ", user '" + user_name + "': " + error;
      return false;
    }
  }
  const std::string cert_data = detail::json_str(*user, "client-certificate-data");
  const std::string key_data = detail::json_str(*user, "client-key-data");
  if (!cert_data.empty() || !key_data.empty()) {
    out.client_cert_pem = detail::base64_decode(cert_data);
    out.client_key_pem = detail::base64_decode(key_data);
    if (out.client_cert_pem.empty() || out.client_key_pem.empty()) {
      error = "Kubeconfig " + kubeconfig_label + ", user '" + user_name + "': client-certificate-data and client-key-data must both be valid base64";
      return false;
    }
  } else {
    const std::string cert_file = detail::resolve_relative(base_dir, detail::json_str(*user, "client-certificate"));
    const std::string key_file = detail::resolve_relative(base_dir, detail::json_str(*user, "client-key"));
    if (!cert_file.empty() || !key_file.empty()) {
      if (!detail::read_file(cert_file, out.client_cert_pem) || !detail::read_file(key_file, out.client_key_pem)) {
        error = "Kubeconfig " + kubeconfig_label + ", user '" + user_name + "': failed to read client-certificate '" + cert_file + "' / client-key '" +
                key_file + "'";
        return false;
      }
    }
  }
  if (insecure && !out.client_cert_pem.empty()) {
    // The HTTP client refuses to present a client certificate to a server it
    // does not authenticate (mTLS without server authentication hands the
    // credential to whoever answers), so this combination could never
    // connect. Say so here, with the fix, instead of at request time.
    error = "Kubeconfig " + kubeconfig_label + ", cluster '" + cluster_name +
            "': insecure-skip-tls-verify cannot be combined with a client certificate (user '" + user_name +
            "'): the agent will not present a certificate to an unverified server. Remove insecure-skip-tls-verify and supply "
            "certificate-authority(-data), or authenticate with a token";
    return false;
  }
  if (out.token.empty() && out.client_cert_pem.empty()) {
    if (user->if_contains("exec") || user->if_contains("auth-provider")) {
      error = "Kubeconfig " + kubeconfig_label + ", user '" + user_name +
              "': exec and auth-provider credentials are not supported; use a service account token (`kubectl create token`) or a client certificate";
    } else {
      error = "Kubeconfig " + kubeconfig_label + ", user '" + user_name + "': no token, tokenFile or client certificate";
    }
    return false;
  }
  return true;
}

// The paths the kubelet projects a service account into.
inline const char *in_cluster_token_path() { return "/var/run/secrets/kubernetes.io/serviceaccount/token"; }
inline const char *in_cluster_ca_path() { return "/var/run/secrets/kubernetes.io/serviceaccount/ca.crt"; }

// Resolution order: explicit `api server`, then `kubeconfig`, then the
// in-cluster environment. False with `error` set when none applies or the one
// that does is incomplete; the message says what to configure.
inline bool resolve_cluster(const settings &s, cluster &out, std::string &error) {
  out = cluster();
  out.verify_mode = s.verify_mode;
  out.tls_version = s.tls_version;
  out.timeout = s.timeout;
  out.max_response_bytes = s.max_response_mb > 0 ? static_cast<std::size_t>(s.max_response_mb) * 1024u * 1024u : 0;
  out.ca = s.ca;

  if (!s.api_server.empty()) {
    out.source = "settings";
    if (!parse_server_url(s.api_server, out, error)) {
      error = "Invalid `api server` under [/settings/kubernetes]: " + error;
      return false;
    }
    if (!s.token_file.empty()) {
      if (!detail::read_token_file(s.token_file, out.token, error)) {
        error = "Invalid `token file` under [/settings/kubernetes]: " + error;
        return false;
      }
    } else {
      out.token = s.token;
    }
    if (out.token.empty()) {
      error = "No credentials for the Kubernetes API server at '" + out.address() + "': set `token` or `token file` under [/settings/kubernetes]";
      return false;
    }
    return true;
  }

  if (!s.kubeconfig.empty()) {
    out.source = "kubeconfig " + s.kubeconfig;
    std::string text;
    if (!detail::read_file(s.kubeconfig, text)) {
      error = "Failed to read kubeconfig '" + s.kubeconfig + "' (`kubeconfig` under [/settings/kubernetes])";
      return false;
    }
    const std::string base_dir = boost::filesystem::path(s.kubeconfig).parent_path().string();
    return resolve_kubeconfig(text, base_dir, s.context, "'" + s.kubeconfig + "'", out, error);
  }

  const char *host = std::getenv("KUBERNETES_SERVICE_HOST");
  if (host && *host) {
    out.source = "in-cluster";
    const char *port = std::getenv("KUBERNETES_SERVICE_PORT");
    out.host = host;
    out.port = (port && *port) ? port : "443";
    if (!detail::read_token_file(in_cluster_token_path(), out.token, error)) {
      error = "Running in-cluster (KUBERNETES_SERVICE_HOST is set) but the service account token is unusable: " + error +
              ". Mount a service account (automountServiceAccountToken) or set `api server` and `token` under [/settings/kubernetes]";
      return false;
    }
    std::string ca_pem;
    if (detail::read_file(in_cluster_ca_path(), ca_pem) && !ca_pem.empty()) out.ca = in_cluster_ca_path();
    return true;
  }

  error =
      "No Kubernetes API server configured: set `api server` and `token` (or `kubeconfig`) under [/settings/kubernetes], or run the agent inside the "
      "cluster with a service account";
  return false;
}

}  // namespace kube_checks

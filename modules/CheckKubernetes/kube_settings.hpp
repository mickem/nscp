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

#include <boost/algorithm/string/trim.hpp>
#include <boost/filesystem.hpp>
#include <boost/json.hpp>
#include <bytes/base64.hpp>
#include <cstdlib>
#include <file_helpers.hpp>
#include <fstream>
#include <json/accessors.hpp>
#include <net/socket/socket_helpers.hpp>
#include <sstream>
#include <string>

namespace kube_checks {

using json_accessors::get_bool;
using json_accessors::get_str;

// Module-level settings, straight from [/settings/kubernetes].
struct settings {
  std::string api_server;  // https://host:6443; empty = in-cluster auto-detect
  std::string token;       // bearer token (redacted in listings: registered with add_password)
  std::string token_file;  // path to a token file, read at check time
  std::string kubeconfig;  // path to a kubeconfig in JSON form
  std::string context;     // kubeconfig context to use; empty = current-context
  std::string ca;          // CA bundle (file or hashed directory); the ${ca-path} default is expanded by the settings layer
  std::string default_ca;  // what ${ca-path} expands to: `ca` still equal to it was not set by the operator
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

// file_helpers::read_file_as_string, with the failure as a return value:
// every caller here turns it into a message naming the setting.
inline bool read_file(const std::string &path, std::string &out) {
  try {
    out = file_helpers::read_file_as_string(path);
    return true;
  } catch (const std::exception &) {
    return false;
  }
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

// A file named by a kubeconfig is relative to the kubeconfig's own
// directory, not to the agent's working directory (client-go resolves it the
// same way, so a kubeconfig that works with kubectl works here).
inline std::string resolve_relative(const std::string &base_dir, const std::string &path) {
  if (path.empty() || base_dir.empty()) return path;
  const boost::filesystem::path p(path);
  // A rooted path ("/etc/ca.pem") is left alone everywhere: on Windows it is
  // not absolute without a drive letter, but it is not relative to the
  // kubeconfig's directory either, and appending would make it so.
  if (p.is_absolute() || p.has_root_directory()) return path;
  return (boost::filesystem::path(base_dir) / p).string();
}

// The named entry of a kubeconfig list ("clusters", "contexts", "users"): the
// object under `section` of the item whose "name" matches. nullptr when absent.
inline const boost::json::object *named_entry(const boost::json::object &root, const char *list, const std::string &name, const char *section) {
  const boost::json::value *l = root.if_contains(list);
  if (!l || !l->is_array()) return nullptr;
  for (const auto &item : l->as_array()) {
    if (!item.is_object()) continue;
    const boost::json::object &o = item.as_object();
    if (get_str(o, "name") != name) continue;
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
  const std::string ctx_name = context_name.empty() ? get_str(cfg, "current-context") : context_name;
  if (ctx_name.empty()) {
    error = "Kubeconfig " + kubeconfig_label + " has no current-context; set `context` under [/settings/kubernetes]";
    return false;
  }
  const boost::json::object *ctx = detail::named_entry(cfg, "contexts", ctx_name, "context");
  if (!ctx) {
    error = "Kubeconfig " + kubeconfig_label + " has no context named '" + ctx_name + "'";
    return false;
  }
  const std::string cluster_name = get_str(*ctx, "cluster");
  const std::string user_name = get_str(*ctx, "user");
  const boost::json::object *cl = detail::named_entry(cfg, "clusters", cluster_name, "cluster");
  if (!cl) {
    error = "Kubeconfig " + kubeconfig_label + ": context '" + ctx_name + "' names a cluster '" + cluster_name + "' that does not exist";
    return false;
  }
  if (!parse_server_url(get_str(*cl, "server"), out, error)) {
    error = "Kubeconfig " + kubeconfig_label + ", cluster '" + cluster_name + "': " + error;
    return false;
  }
  const std::string ca_data = get_str(*cl, "certificate-authority-data");
  if (!ca_data.empty()) {
    out.ca_pem = bytes::base64_decode(ca_data);
    if (out.ca_pem.empty()) {
      error = "Kubeconfig " + kubeconfig_label + ", cluster '" + cluster_name + "': certificate-authority-data is not valid base64";
      return false;
    }
  } else {
    const std::string ca_file = get_str(*cl, "certificate-authority");
    if (!ca_file.empty()) out.ca = detail::resolve_relative(base_dir, ca_file);
  }
  const bool insecure = get_bool(*cl, "insecure-skip-tls-verify");
  if (insecure && !out.ca_pem.empty()) {
    // The CA data would pin the server and the client verifies against a
    // pin regardless of the verify mode, so the flag would be silently
    // ignored. client-go rejects the pair; so does this.
    error = "Kubeconfig " + kubeconfig_label + ", cluster '" + cluster_name +
            "': insecure-skip-tls-verify cannot be combined with certificate-authority-data. Remove one of them: the CA data verifies the server, the "
            "flag says not to";
    return false;
  }
  if (insecure) out.verify_mode = "none";

  const boost::json::object *user = detail::named_entry(cfg, "users", user_name, "user");
  if (!user) {
    error = "Kubeconfig " + kubeconfig_label + ": context '" + ctx_name + "' names a user '" + user_name + "' that does not exist";
    return false;
  }
  out.token = get_str(*user, "token");
  if (out.token.empty()) {
    const std::string token_file = detail::resolve_relative(base_dir, get_str(*user, "tokenFile"));
    if (!token_file.empty() && !detail::read_token_file(token_file, out.token, error)) {
      error = "Kubeconfig " + kubeconfig_label + ", user '" + user_name + "': " + error;
      return false;
    }
  }
  const std::string cert_data = get_str(*user, "client-certificate-data");
  const std::string key_data = get_str(*user, "client-key-data");
  if (!cert_data.empty() || !key_data.empty()) {
    out.client_cert_pem = bytes::base64_decode(cert_data);
    out.client_key_pem = bytes::base64_decode(key_data);
    if (out.client_cert_pem.empty() || out.client_key_pem.empty()) {
      error = "Kubeconfig " + kubeconfig_label + ", user '" + user_name + "': client-certificate-data and client-key-data must both be valid base64";
      return false;
    }
  } else {
    const std::string cert_file = detail::resolve_relative(base_dir, get_str(*user, "client-certificate"));
    const std::string key_file = detail::resolve_relative(base_dir, get_str(*user, "client-key"));
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
  if (s.timeout <= 0) {
    // The HTTP client reads 0 as "no deadline", which would let one stalled
    // API server hold a check (and the thread running it) forever.
    error =
        "Invalid `timeout` under [/settings/kubernetes]: " + std::to_string(s.timeout) + " - give the deadline for each request in seconds, a positive number";
    return false;
  }
  if (s.max_response_mb < 0) {
    error = "Invalid `max response size` under [/settings/kubernetes]: " + std::to_string(s.max_response_mb) + " - give the cap in megabytes, or 0 for no cap";
    return false;
  }
  out.max_response_bytes = static_cast<std::size_t>(s.max_response_mb) * 1024u * 1024u;
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
    if (!resolve_kubeconfig(text, base_dir, s.context, "'" + s.kubeconfig + "'", out, error)) return false;
    if (!out.ca_pem.empty() && socket_helpers::client_verify_mode_disables_verification(out.verify_mode)) {
      // The same trap as insecure-skip-tls-verify in the kubeconfig: the CA
      // data is handed to the client as a pin, and the client verifies
      // against a pin whatever the verify mode says, so `none` would be
      // silently ignored.
      error = "Kubeconfig '" + s.kubeconfig + "': `verify mode = " + out.verify_mode +
              "` under [/settings/kubernetes] cannot be combined with certificate-authority-data. Remove one of them: the "
              "CA data verifies the server, the verify mode says not to";
      return false;
    }
    return true;
  }

  const char *host = std::getenv("KUBERNETES_SERVICE_HOST");
  if (host && *host) {
    out.source = "in-cluster";
    const char *port = std::getenv("KUBERNETES_SERVICE_PORT");
    out.host = host;
    out.port = (port && *port) ? port : "443";
    // A configured credential wins over the mounted one: an operator who set
    // `token file` (a projected token for a different service account, say)
    // meant it, even with the server auto-detected.
    if (!s.token_file.empty()) {
      if (!detail::read_token_file(s.token_file, out.token, error)) {
        error = "Invalid `token file` under [/settings/kubernetes]: " + error;
        return false;
      }
    } else if (!s.token.empty()) {
      out.token = s.token;
    } else if (!detail::read_token_file(in_cluster_token_path(), out.token, error)) {
      error = "Running in-cluster (KUBERNETES_SERVICE_HOST is set) but the service account token is unusable: " + error +
              ". Mount a service account (automountServiceAccountToken) or set `api server` and `token` under [/settings/kubernetes]";
      return false;
    }
    // The mounted CA only stands in for the default: a `ca` the operator
    // set (a bundle that also trusts a TLS-intercepting mesh, say) wins, the
    // same way a configured token wins over the mounted one.
    const bool ca_configured = !s.ca.empty() && s.ca != s.default_ca;
    std::string ca_pem;
    if (!ca_configured && detail::read_file(in_cluster_ca_path(), ca_pem) && !ca_pem.empty()) out.ca = in_cluster_ca_path();
    return true;
  }

  error =
      "No Kubernetes API server configured: set `api server` and `token` (or `kubeconfig`) under [/settings/kubernetes], or run the agent inside the "
      "cluster with a service account";
  return false;
}

}  // namespace kube_checks

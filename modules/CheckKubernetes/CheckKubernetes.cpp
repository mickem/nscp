// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "CheckKubernetes.h"

#include <boost/algorithm/string/trim.hpp>
#include <memory>
#include <mutex>
#include <net/http/client.hpp>
#include <net/socket/socket_helpers.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/settings/helper.hpp>
#include <nscapi/settings/proxy.hpp>
#include <str/utf8.hpp>

#include "check_cluster.hpp"
#include "check_nodes.hpp"
#include "check_pods.hpp"
#include "check_workloads.hpp"
#include "kube_client.hpp"

namespace sh = nscapi::settings_helper;

// The real transport: HTTPS to the resolved API server with the bearer token.
// Everything about where the request goes and what it carries comes from the
// resolved cluster, never from the check request.
//
// One client serves every request of a check invocation: the TLS context
// (CA bundle, client certificate) is built once, and each page of a list or
// each namespace reconnects through it rather than reloading the bundle and
// re-parsing the options.
kube_checks::fetcher CheckKubernetes::make_api_fetcher(const kube_checks::cluster &target) {
  if (target.protocol == "http" && !target.token.empty() && first_warning_for(target.address())) {
    // Once per server, not once per check: this runs on every invocation, and
    // a line repeated every minute buries the log it is meant to stand out in.
    NSC_LOG_WARNING("The Kubernetes API server at '" + target.address() + "' (" + target.source +
                    ") is reached over plain http: the bearer token is sent in cleartext to anyone on the path. Use https unless this is a local "
                    "proxy on a trusted link. This is logged once per server while the service runs.");
  }
  http::http_client_options options(target.protocol, target.tls_version, target.verify_mode, target.ca);
  // resolve_cluster and the checks refuse a non-positive timeout: 0 here
  // would mean no deadline at all.
  options.timeout_seconds_ = static_cast<unsigned int>(target.timeout);
  options.max_response_bytes_ = target.max_response_bytes;
  // A kubeconfig's certificate-authority-data is a CA, so the client keeps
  // hostname verification and merely adds it as a trust root.
  options.identity_.pinned_ca_pem = target.ca_pem;
  options.identity_.cert_pem = target.client_cert_pem;
  options.identity_.key_pem = target.client_key_pem;
  std::shared_ptr<http::simple_client> client;
  try {
    client = std::make_shared<http::simple_client>(options);
  } catch (const socket_helpers::socket_exception &e) {
    // what() is the caller-safe half ("see the agent log for the reason");
    // the OpenSSL reason is only in the detail, so it has to be logged here
    // or it is lost. The check reports what() through open_fetcher.
    if (e.has_detail()) NSC_LOG_ERROR_STD(e.detail());
    throw;
  }
  return [target, client](const std::string &path) -> std::string {
    http::request rq("GET", target.host_header(), target.base_path + path);
    rq.add_header("Accept", "application/json");
    rq.add_header("User-Agent", "NSClient++ CheckKubernetes");
    if (!target.token.empty()) rq.add_header("Authorization", "Bearer " + target.token);
    // fetch(), not execute(): execute() throws on any non-2xx, folding a 403
    // (RBAC) into the same failure as an unreachable server. fetch() hands
    // back the status and the Status body so the check can say what to fix.
    http::response resp;
    try {
      resp = client->fetch(target.host, target.port, rq);
    } catch (const socket_helpers::socket_exception &e) {
      if (e.has_detail()) NSC_LOG_ERROR_STD(e.detail());
      throw;
    }
    if (!resp.is_2xx()) {
      // The status message comes off the status line with its leading
      // space and trailing carriage return; the check renders it inside a
      // one-line plugin message when the body is not a Status object.
      std::string reason = resp.status_message_;
      boost::algorithm::trim(reason);
      throw kube_checks::kube_http_error(resp.status_code_, "HTTP " + std::to_string(resp.status_code_) + " " + reason, resp.payload_);
    }
    return resp.payload_;
  };
}

// Bounded, as in the NRDP client: a settings reload can point the module at
// a new server, and a warning repeated after a wrap beats a set that only
// grows.
bool CheckKubernetes::first_warning_for(const std::string &key) {
  static constexpr std::size_t kMaxRemembered = 64;
  std::lock_guard<std::mutex> guard(warned_mutex_);
  if (warned_targets_.size() >= kMaxRemembered) warned_targets_.clear();
  return warned_targets_.insert(key).second;
}

bool CheckKubernetes::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
  try {
    sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
    settings.set_alias(alias, "kubernetes");

    // clang-format off
    settings.alias().add_key_to_settings()
      .add_string("api server", sh::string_key(&defaults_.api_server, ""),
        "API SERVER", "The Kubernetes API server, e.g. https://k8s.example.com:6443. Leave empty to auto-detect when the agent runs inside the cluster (KUBERNETES_SERVICE_HOST plus the mounted service account token and CA). Which cluster the agent talks to is an operator decision: a check request cannot choose another server. Use https: an http:// server receives the bearer token in cleartext, and the agent logs a warning when it connects to one.")
      .add_password("token", sh::string_key(&defaults_.token, ""),
        "BEARER TOKEN", "Service account bearer token used to authenticate against the API server. Prefer `token file` for a token that is rotated.")
      .add_string("token file", sh::path_key(&defaults_.token_file, ""),
        "TOKEN FILE", "Path to a file holding the bearer token, read at check time so a rotated (projected) token keeps working without a reload. Takes precedence over `token`.")
      .add_string("kubeconfig", sh::path_key(&defaults_.kubeconfig, ""),
        "KUBECONFIG (JSON)", "Path to a kubeconfig in JSON form (`kubectl config view --raw --minify -o json > nscp-kubeconfig.json`), used when `api server` is empty. YAML kubeconfigs are not read. Supplies the server, CA, token or client certificate of the selected context.", true)
      .add_string("context", sh::string_key(&defaults_.context, ""),
        "KUBECONFIG CONTEXT", "The kubeconfig context to use; empty means its current-context.", true)
      .add_string("ca", sh::path_key(&defaults_.ca, "${ca-path}"),
        "CERTIFICATE AUTHORITY", "CA bundle (a PEM file or a hashed directory) used to verify the API server certificate. Defaults to the trusted system store (in-cluster: the mounted service account CA, unless this is set); point it at the cluster CA (`kubectl config view --raw -o jsonpath='{.clusters[0].cluster.certificate-authority-data}' | base64 -d`) for a private cluster CA.")
      .add_string("verify mode", sh::string_key(&defaults_.verify_mode, "peer"),
        "TLS PEER VERIFY MODE", "Comma separated list of options: none, peer (or certificate), peer-cert, fail-if-no-cert (or fail-if-no-peer-cert, client-certificate). For a self signed certificate use peer-cert and point `ca` at that certificate; none disables verification entirely and sends the token to an unverified peer.")
      .add_string("tls version", sh::string_key(&defaults_.tls_version, "tlsv1.2+"),
        "TLS VERSION", "Minimum TLS protocol version accepted: tlsv1.0, tlsv1.1, tlsv1.2, tlsv1.2+ (the default: TLS 1.2 and 1.3), tlsv1.3.", true)
      .add_int("timeout", sh::int_key(&defaults_.timeout, 30),
        "TIMEOUT", "Timeout for each API server request, in seconds. Must be positive: the checks refuse 0 or less rather than wait forever.", true)
      .add_int("max response size", sh::int_key(&defaults_.max_response_mb, 64),
        "MAX RESPONSE SIZE", "Largest API response the agent will buffer, in megabytes. A pod list in a large cluster runs to tens of megabytes; 0 removes the cap.", true)
      ;
    // clang-format on

    settings.register_all();
    settings.notify();
    // `ca` defaults to the system store; in-cluster, the mounted service
    // account CA replaces that default but never a bundle the operator set.
    defaults_.default_ca = get_core()->expand_path("${ca-path}");
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("loading: ", e);
    return false;
  } catch (...) {
    NSC_LOG_ERROR_EX("loading: ");
    return false;
  }
  return true;
}

bool CheckKubernetes::unloadModule() { return true; }

void CheckKubernetes::check_kubernetes(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  kube_checks::check_cluster(defaults_, request, response, api_fetcher_factory());
}

void CheckKubernetes::check_pods(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  kube_checks::check_pods(defaults_, request, response, api_fetcher_factory());
}

void CheckKubernetes::check_nodes(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  kube_checks::check_nodes(defaults_, request, response, api_fetcher_factory());
}

void CheckKubernetes::check_workloads(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  kube_checks::check_workloads(defaults_, request, response, api_fetcher_factory());
}

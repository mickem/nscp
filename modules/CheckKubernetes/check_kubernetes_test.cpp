// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <cstdlib>
#include <fstream>
#include <map>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#include "check_cluster.hpp"
#include "check_nodes.hpp"
#include "check_pods.hpp"
#include "check_workloads.hpp"
#include "kube_client.hpp"
#include "kube_settings.hpp"

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
static nscapi::helper_singleton test_plugin_singleton;
nscapi::helper_singleton *nscapi::plugin_singleton = &test_plugin_singleton;

namespace {

const char *const TOKEN = "secret-token-value";

std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

std::string perf_of(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    for (int j = 0; j < r.lines(i).perf_size(); ++j) {
      const auto &p = r.lines(i).perf(j);
      if (!out.empty()) out += " ";
      out += p.alias() + "=" + std::to_string(static_cast<long long>(p.float_value().value()));
    }
  }
  return out;
}

// An API server serving canned payloads by path. A route keyed by the exact
// path including its query string wins over one keyed by the bare path, which
// is how pagination is scripted (the `continue` parameter selects the page).
struct fake_api {
  std::map<std::string, std::string> routes;
  std::map<std::string, std::pair<long, std::string>> errors;  // path -> (status, body)
  std::vector<std::string> requests;
  kube_checks::cluster last_target;

  void serve(const std::string &path, const std::string &payload) { routes[path] = payload; }
  void refuse(const std::string &path, const long status, const std::string &body = "") { errors[path] = std::make_pair(status, body); }

  kube_checks::fetcher_factory factory() {
    return [this](const kube_checks::cluster &target) -> kube_checks::fetcher {
      last_target = target;
      return [this](const std::string &path) -> std::string {
        requests.push_back(path);
        const std::string bare = path.substr(0, path.find('?'));
        for (const std::string &key : {path, bare}) {
          const auto err = errors.find(key);
          if (err != errors.end()) {
            throw kube_checks::kube_http_error(err->second.first, "HTTP " + std::to_string(err->second.first), err->second.second);
          }
          const auto it = routes.find(key);
          if (it != routes.end()) return it->second;
        }
        throw kube_checks::kube_http_error(404, "HTTP 404 Not Found", R"({"kind":"Status","message":"the server could not find the requested resource"})");
      };
    };
  }
};

// A factory whose client cannot even be constructed: the TLS context is built
// eagerly, so a missing CA file or non-PEM identity data throws here.
kube_checks::fetcher_factory unconstructible_api(const std::string &error) {
  return [error](const kube_checks::cluster &) -> kube_checks::fetcher { throw std::runtime_error(error); };
}

kube_checks::fetcher_factory unreachable_api(const std::string &error) {
  return [error](const kube_checks::cluster &) -> kube_checks::fetcher {
    return [error](const std::string &) -> std::string { throw std::runtime_error(error); };
  };
}

kube_checks::settings configured() {
  kube_checks::settings s;
  s.api_server = "https://k8s.example.com:6443";
  s.token = TOKEN;
  return s;
}

PB::Common::ResultCode run_pods(const kube_checks::fetcher_factory &factory, const std::vector<std::string> &args,
                                PB::Commands::QueryResponseMessage::Response &response, const kube_checks::settings &defaults = configured()) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_pods");
  for (const std::string &a : args) request.add_arguments(a);
  kube_checks::check_pods(defaults, request, &response, factory);
  return response.result();
}

PB::Common::ResultCode run_nodes(const kube_checks::fetcher_factory &factory, const std::vector<std::string> &args,
                                 PB::Commands::QueryResponseMessage::Response &response, const kube_checks::settings &defaults = configured()) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_nodes");
  for (const std::string &a : args) request.add_arguments(a);
  kube_checks::check_nodes(defaults, request, &response, factory);
  return response.result();
}

PB::Common::ResultCode run_workloads(const kube_checks::fetcher_factory &factory, const std::vector<std::string> &args,
                                     PB::Commands::QueryResponseMessage::Response &response, const kube_checks::settings &defaults = configured()) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_workloads");
  for (const std::string &a : args) request.add_arguments(a);
  kube_checks::check_workloads(defaults, request, &response, factory);
  return response.result();
}

PB::Common::ResultCode run_cluster(const kube_checks::fetcher_factory &factory, const std::vector<std::string> &args,
                                   PB::Commands::QueryResponseMessage::Response &response, const kube_checks::settings &defaults = configured()) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_kubernetes");
  for (const std::string &a : args) request.add_arguments(a);
  kube_checks::check_cluster(defaults, request, &response, factory);
  return response.result();
}

const char *VERSION = R"json({"major":"1","minor":"30","gitVersion":"v1.30.2","platform":"linux/amd64"})json";

std::string node(const std::string &name, const std::string &ready) {
  return R"({"metadata":{"name":")" + name + R"("},"status":{"conditions":[{"type":"Ready","status":")" + ready + R"("}]}})";
}

const std::string THREE_NODES =
    R"({"kind":"NodeList","metadata":{},"items":[)" + node("cp-1", "True") + "," + node("worker-1", "True") + "," + node("worker-2", "False") + "]}";

// A scratch file that is removed with the test.
struct temp_file {
  boost::filesystem::path path;
  explicit temp_file(const std::string &content) {
    path = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("nscp-kube-%%%%-%%%%.tmp");
    std::ofstream out(path.string().c_str(), std::ios::binary);
    out << content;
  }
  ~temp_file() {
    boost::system::error_code ec;
    boost::filesystem::remove(path, ec);
  }
};

struct scoped_env {
  std::string key;
  explicit scoped_env(const std::string &k, const std::string &value) : key(k) {
#ifdef WIN32
    _putenv_s(key.c_str(), value.c_str());
#else
    setenv(key.c_str(), value.c_str(), 1);
#endif
  }
  ~scoped_env() {
#ifdef WIN32
    _putenv_s(key.c_str(), "");
#else
    unsetenv(key.c_str());
#endif
  }
};

}  // namespace

// --- check_kubernetes ---------------------------------------------------------

TEST(CheckKubernetes, HealthyClusterIsOk) {
  fake_api api;
  api.serve("/version", VERSION);
  api.serve("/readyz", "ok\n");
  api.serve("/api/v1/nodes", THREE_NODES);
  PB::Commands::QueryResponseMessage::Response response;
  // Perfdata follows the thresholded keywords, as in every filter check.
  EXPECT_EQ(run_cluster(api.factory(), {"warning=nodes_not_ready > 1", "critical=nodes_ready < 1"}, response), PB::Common::ResultCode::OK)
      << join_lines(response);
  EXPECT_EQ(join_lines(response), "OK: Kubernetes v1.30.2 at https://k8s.example.com:6443: API ready, 2/3 nodes ready");
  EXPECT_NE(perf_of(response).find("https://k8s.example.com:6443 not ready nodes=1"), std::string::npos) << perf_of(response);
  EXPECT_NE(perf_of(response).find("https://k8s.example.com:6443 ready nodes=2"), std::string::npos) << perf_of(response);
  ASSERT_EQ(api.requests.size(), 3u);
  EXPECT_EQ(api.requests[0], "/version");
  EXPECT_EQ(api.requests[1], "/readyz");
  EXPECT_EQ(api.requests[2], "/api/v1/nodes?limit=500");
  EXPECT_EQ(api.last_target.token, TOKEN);
  EXPECT_EQ(api.last_target.host, "k8s.example.com");
  EXPECT_EQ(api.last_target.port, "6443");
}

TEST(CheckKubernetes, ReadyzFailureIsCriticalAndNamesTheCheck) {
  fake_api api;
  api.serve("/version", VERSION);
  api.refuse("/readyz", 500, "[+]ping ok\n[-]etcd failed: reason withheld\n[+]poststarthook/x ok\nreadyz check failed\n");
  api.serve("/api/v1/nodes", THREE_NODES);
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_cluster(api.factory(), {"detail-syntax=%(api_ready)|%(readyz)|%(api_ready)", "top-syntax=${status}: ${list}"}, response),
            PB::Common::ResultCode::CRITICAL)
      << join_lines(response);
  EXPECT_EQ(join_lines(response), "CRITICAL: not ready|failed: etcd|not ready");
}

TEST(CheckKubernetes, NodeThresholdsAndKeywords) {
  fake_api api;
  api.serve("/version", VERSION);
  api.serve("/readyz", "ok");
  api.serve("/api/v1/nodes", THREE_NODES);
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_cluster(api.factory(), {"warning=nodes_not_ready > 0", "detail-syntax=%(version) %(platform) %(source) %(nodes_not_ready)"}, response),
            PB::Common::ResultCode::WARNING)
      << join_lines(response);
  EXPECT_EQ(join_lines(response), "WARNING: v1.30.2 linux/amd64 settings 1");
}

TEST(CheckKubernetes, NodeListIsPaginated) {
  fake_api api;
  api.serve("/version", VERSION);
  api.serve("/readyz", "ok");
  api.serve("/api/v1/nodes?limit=500", R"({"metadata":{"continue":"page-2"},"items":[)" + node("a", "True") + "]}");
  api.serve("/api/v1/nodes?limit=500&continue=page-2", R"({"metadata":{"continue":""},"items":[)" + node("b", "True") + "," + node("c", "Unknown") + "]}");
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_cluster(api.factory(), {}, response), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_NE(join_lines(response).find("2/3 nodes ready"), std::string::npos) << join_lines(response);
  ASSERT_EQ(api.requests.size(), 4u);
  EXPECT_EQ(api.requests[3], "/api/v1/nodes?limit=500&continue=page-2");
}

TEST(CheckKubernetes, RejectedTokenIsUnknownWithoutTheToken) {
  fake_api api;
  api.refuse("/version", 401, R"({"kind":"Status","message":"Unauthorized","reason":"Unauthorized","code":401})");
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_cluster(api.factory(), {}, response), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
  const std::string msg = join_lines(response);
  EXPECT_NE(msg.find("Kubernetes API server at 'https://k8s.example.com:6443' rejected the credentials (HTTP 401 for GET /version)"), std::string::npos) << msg;
  EXPECT_EQ(msg.find(TOKEN), std::string::npos) << msg;
}

TEST(CheckKubernetes, ForbiddenIsUnknownAndNamesTheRbacRule) {
  fake_api api;
  api.serve("/version", VERSION);
  api.serve("/readyz", "ok");
  api.refuse(
      "/api/v1/nodes", 403,
      R"({"kind":"Status","status":"Failure","message":"nodes is forbidden: User \"system:serviceaccount:monitoring:nscp\" cannot list resource \"nodes\" in API group \"\" at the cluster scope","reason":"Forbidden","code":403})");
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_cluster(api.factory(), {}, response), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
  const std::string msg = join_lines(response);
  EXPECT_NE(
      msg.find(
          "denied GET /api/v1/nodes?limit=500 (HTTP 403: nodes is forbidden: User \"system:serviceaccount:monitoring:nscp\" cannot list resource \"nodes\""),
      std::string::npos)
      << msg;
  EXPECT_NE(msg.find("grant the agent's service account get and list on the resource"), std::string::npos) << msg;
  EXPECT_EQ(msg.find(TOKEN), std::string::npos) << msg;
}

TEST(CheckKubernetes, ANegativeResponseCapIsRefusedNotUnlimited) {
  fake_api api;
  kube_checks::settings s = configured();
  s.max_response_mb = -1;
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_cluster(api.factory(), {}, response, s), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
  EXPECT_NE(join_lines(response).find("Invalid `max response size` under [/settings/kubernetes]: -1"), std::string::npos) << join_lines(response);
  EXPECT_TRUE(api.requests.empty());
  s.max_response_mb = 0;
  PB::Commands::QueryResponseMessage::Response response2;
  run_cluster(api.factory(), {}, response2, s);
  EXPECT_EQ(api.last_target.max_response_bytes, 0u) << "0 is the documented no-cap value";
}

TEST(CheckKubernetes, UnreachableServerIsUnknown) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_cluster(unreachable_api("Failed to connect to k8s.example.com:6443: Connection refused"), {}, response), PB::Common::ResultCode::UNKNOWN)
      << join_lines(response);
  EXPECT_EQ(
      join_lines(response),
      "Failed to connect to Kubernetes API server at 'https://k8s.example.com:6443' (settings): Failed to connect to k8s.example.com:6443: Connection refused");
}

TEST(CheckKubernetes, ABadTlsConfigurationIsUnknownNotACommandFailure) {
  // The client's constructor loads the CA bundle and parses the identity;
  // that failure has to land under the same contract as a refused
  // connection, on every command.
  const std::string error = "Failed to load CA /etc/nscp/missing-ca.pem: no such file";
  PB::Commands::QueryResponseMessage::Response cluster;
  EXPECT_EQ(run_cluster(unconstructible_api(error), {}, cluster), PB::Common::ResultCode::UNKNOWN) << join_lines(cluster);
  EXPECT_EQ(join_lines(cluster), "Failed to connect to Kubernetes API server at 'https://k8s.example.com:6443' (settings): " + error);
  PB::Commands::QueryResponseMessage::Response pods;
  EXPECT_EQ(run_pods(unconstructible_api(error), {}, pods), PB::Common::ResultCode::UNKNOWN) << join_lines(pods);
  EXPECT_NE(join_lines(pods).find(error), std::string::npos);
  PB::Commands::QueryResponseMessage::Response nodes;
  EXPECT_EQ(run_nodes(unconstructible_api(error), {}, nodes), PB::Common::ResultCode::UNKNOWN) << join_lines(nodes);
  EXPECT_NE(join_lines(nodes).find(error), std::string::npos);
  PB::Commands::QueryResponseMessage::Response workloads;
  EXPECT_EQ(run_workloads(unconstructible_api(error), {}, workloads), PB::Common::ResultCode::UNKNOWN) << join_lines(workloads);
  EXPECT_NE(join_lines(workloads).find(error), std::string::npos);
}

TEST(CheckKubernetes, ReadyzUnavailableIsNotAVerdict) {
  // A 404 from a path-restricting ingress, or a 502 HTML page, is the path
  // being unavailable; /version and the node list answered, so the API is
  // reachable and nothing said it is unready.
  fake_api api;
  api.serve("/version", VERSION);
  api.serve("/api/v1/nodes", THREE_NODES);
  api.refuse("/readyz", 404, R"({"kind":"Status","message":"the server could not find the requested resource","code":404})");
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_cluster(api.factory(), {"detail-syntax=%(api_ready)|%(readyz)"}, response), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_EQ(join_lines(response), "OK: ready|unavailable (HTTP 404)");

  api.refuse("/readyz", 502, "<html><body><h1>502 Bad Gateway</h1></body></html>");
  PB::Commands::QueryResponseMessage::Response response2;
  EXPECT_EQ(run_cluster(api.factory(), {"detail-syntax=%(api_ready)|%(readyz)"}, response2), PB::Common::ResultCode::OK) << join_lines(response2);
  EXPECT_EQ(join_lines(response2), "OK: ready|unavailable (HTTP 502)");

  // The API server's own report, without a named check, is still a verdict.
  api.refuse("/readyz", 500, "readyz check failed\n");
  PB::Commands::QueryResponseMessage::Response response3;
  EXPECT_EQ(run_cluster(api.factory(), {"detail-syntax=%(api_ready)|%(readyz)"}, response3), PB::Common::ResultCode::CRITICAL) << join_lines(response3);
  EXPECT_EQ(join_lines(response3), "CRITICAL: not ready|failed (HTTP 500)");
}

TEST(CheckKubernetes, MissingConfigurationIsUnknownBeforeAnyRequest) {
  fake_api api;
  PB::Commands::QueryResponseMessage::Response response;
  kube_checks::settings none;
  EXPECT_EQ(run_cluster(api.factory(), {}, response, none), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
  EXPECT_NE(join_lines(response).find("No Kubernetes API server configured"), std::string::npos) << join_lines(response);
  EXPECT_TRUE(api.requests.empty());
}

TEST(CheckKubernetes, ReadyzDeniedByRbacIsUnknownNotCritical) {
  // A service account without the nonResourceURLs rule for /readyz must not
  // page anyone about a healthy cluster: the contract for 401/403 is UNKNOWN
  // with the rule to grant, on every path.
  fake_api api;
  api.serve("/version", VERSION);
  api.refuse(
      "/readyz", 403,
      R"({"kind":"Status","message":"forbidden: User \"system:serviceaccount:monitoring:nscp\" cannot get path \"/readyz\"","reason":"Forbidden","code":403})");
  api.serve("/api/v1/nodes", THREE_NODES);
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_cluster(api.factory(), {}, response), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
  const std::string msg = join_lines(response);
  EXPECT_NE(msg.find("denied GET /readyz (HTTP 403: forbidden: User \"system:serviceaccount:monitoring:nscp\" cannot get path \"/readyz\")"), std::string::npos)
      << msg;
  EXPECT_NE(msg.find("grant the agent's service account get on the non-resource URL /readyz (a nonResourceURLs rule)"), std::string::npos)
      << "a non-resource URL wants a nonResourceURLs rule, not a resource grant: " << msg;
  EXPECT_EQ(response.lines_size(), 1);

  api.refuse("/readyz", 401, "");
  PB::Commands::QueryResponseMessage::Response response2;
  EXPECT_EQ(run_cluster(api.factory(), {}, response2), PB::Common::ResultCode::UNKNOWN) << join_lines(response2);
  EXPECT_NE(join_lines(response2).find("rejected the credentials (HTTP 401 for GET /readyz)"), std::string::npos) << join_lines(response2);
}

TEST(CheckKubernetes, TimeoutOptionReachesTheFetcher) {
  fake_api api;
  api.serve("/version", VERSION);
  api.serve("/readyz", "ok");
  api.serve("/api/v1/nodes", THREE_NODES);
  PB::Commands::QueryResponseMessage::Response response;
  run_cluster(api.factory(), {"timeout=7"}, response);
  EXPECT_EQ(api.last_target.timeout, 7);
}

TEST(CheckKubernetes, NonPositiveTimeoutIsRefused) {
  // The HTTP client reads 0 as "no deadline"; a check must not hand it one.
  for (const std::string &value : {"0", "-5"}) {
    fake_api api;
    api.serve("/version", VERSION);
    PB::Commands::QueryResponseMessage::Response response;
    EXPECT_EQ(run_cluster(api.factory(), {"timeout=" + value}, response), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
    EXPECT_NE(join_lines(response).find("Invalid timeout=" + value + ": give the deadline for each API server request in seconds"), std::string::npos)
        << join_lines(response);
    EXPECT_TRUE(api.requests.empty()) << "nothing is fetched without a deadline";
  }
}

// --- settings resolution --------------------------------------------------------

TEST(KubeSettings, ServerUrlForms) {
  kube_checks::cluster c;
  std::string error;
  ASSERT_TRUE(kube_checks::parse_server_url("https://k8s.example.com:6443", c, error)) << error;
  EXPECT_EQ(c.protocol, "https");
  EXPECT_EQ(c.host, "k8s.example.com");
  EXPECT_EQ(c.port, "6443");
  EXPECT_EQ(c.base_path, "");
  EXPECT_EQ(c.address(), "https://k8s.example.com:6443");

  ASSERT_TRUE(kube_checks::parse_server_url("https://rancher.example.com/k8s/clusters/c-m-abc/", c, error)) << error;
  EXPECT_EQ(c.port, "443");
  EXPECT_EQ(c.base_path, "/k8s/clusters/c-m-abc");
  EXPECT_EQ(c.address(), "https://rancher.example.com/k8s/clusters/c-m-abc");
  EXPECT_EQ(c.host_header(), "rancher.example.com");

  ASSERT_TRUE(kube_checks::parse_server_url("https://[fd00::1]:6443", c, error)) << error;
  EXPECT_EQ(c.host, "fd00::1");
  EXPECT_EQ(c.port, "6443");
  EXPECT_EQ(c.host_header(), "[fd00::1]:6443");

  ASSERT_TRUE(kube_checks::parse_server_url("10.0.0.1:6443", c, error)) << error;
  EXPECT_EQ(c.protocol, "https");
  EXPECT_EQ(c.host, "10.0.0.1");

  EXPECT_FALSE(kube_checks::parse_server_url("ftp://x", c, error));
  EXPECT_FALSE(kube_checks::parse_server_url("https://", c, error));
  EXPECT_FALSE(kube_checks::parse_server_url("https://host:abc", c, error));
}

TEST(KubeSettings, ExplicitSettingsWithTokenFile) {
  const temp_file token("  file-token \n");
  kube_checks::settings s = configured();
  s.token_file = token.path.string();
  s.timeout = 12;
  s.max_response_mb = 2;
  s.verify_mode = "peer-cert";
  s.ca = "/etc/nscp/k8s-ca.pem";
  kube_checks::cluster c;
  std::string error;
  ASSERT_TRUE(kube_checks::resolve_cluster(s, c, error)) << error;
  EXPECT_EQ(c.token, "file-token") << "the token file wins over `token` and is trimmed";
  EXPECT_EQ(c.timeout, 12);
  EXPECT_EQ(c.max_response_bytes, 2u * 1024u * 1024u);
  EXPECT_EQ(c.verify_mode, "peer-cert");
  EXPECT_EQ(c.ca, "/etc/nscp/k8s-ca.pem");
  EXPECT_EQ(c.source, "settings");

  s.token_file = "/nonexistent/nscp/token";
  EXPECT_FALSE(kube_checks::resolve_cluster(s, c, error));
  EXPECT_NE(error.find("Failed to read token file"), std::string::npos) << error;

  s.token_file.clear();
  s.timeout = 0;
  EXPECT_FALSE(kube_checks::resolve_cluster(s, c, error));
  EXPECT_NE(error.find("Invalid `timeout` under [/settings/kubernetes]: 0"), std::string::npos) << error;
  s.timeout = 30;

  s.token_file.clear();
  s.token.clear();
  EXPECT_FALSE(kube_checks::resolve_cluster(s, c, error));
  EXPECT_NE(error.find("set `token` or `token file`"), std::string::npos) << error;
}

const char *KUBECONFIG = R"json({
  "apiVersion": "v1", "kind": "Config",
  "current-context": "prod",
  "clusters": [
    {"name": "prod-cluster", "cluster": {"server": "https://prod.example.com:6443", "certificate-authority-data": "LS0tLS1CRUdJTiBDRVJUSUZJQ0FURS0tLS0tCg=="}},
    {"name": "lab-cluster", "cluster": {"server": "https://lab.example.com", "certificate-authority-data": "LS0tLS1CRUdJTiBDRVJUSUZJQ0FURS0tLS0tCg=="}},
    {"name": "insecure-cluster", "cluster": {"server": "https://insecure.example.com", "insecure-skip-tls-verify": true}},
    {"name": "insecure-ca-cluster", "cluster": {"server": "https://insecure.example.com", "insecure-skip-tls-verify": true, "certificate-authority-data": "LS0tLS1CRUdJTiBDRVJUSUZJQ0FURS0tLS0tCg=="}}
  ],
  "contexts": [
    {"name": "prod", "context": {"cluster": "prod-cluster", "user": "prod-user"}},
    {"name": "lab", "context": {"cluster": "lab-cluster", "user": "lab-user"}},
    {"name": "insecure", "context": {"cluster": "insecure-cluster", "user": "prod-user"}},
    {"name": "insecure-cert", "context": {"cluster": "insecure-cluster", "user": "lab-user"}},
    {"name": "insecure-ca", "context": {"cluster": "insecure-ca-cluster", "user": "prod-user"}},
    {"name": "cloud", "context": {"cluster": "lab-cluster", "user": "cloud-user"}}
  ],
  "users": [
    {"name": "prod-user", "user": {"token": "prod-token"}},
    {"name": "lab-user", "user": {"client-certificate-data": "Q0VSVA==", "client-key-data": "S0VZ"}},
    {"name": "cloud-user", "user": {"exec": {"command": "aws"}}}
  ]
})json";

TEST(KubeSettings, KubeconfigCurrentContext) {
  const temp_file cfg(KUBECONFIG);
  kube_checks::settings s;
  s.kubeconfig = cfg.path.string();
  kube_checks::cluster c;
  std::string error;
  ASSERT_TRUE(kube_checks::resolve_cluster(s, c, error)) << error;
  EXPECT_EQ(c.host, "prod.example.com");
  EXPECT_EQ(c.port, "6443");
  EXPECT_EQ(c.token, "prod-token");
  EXPECT_EQ(c.ca_pem, "-----BEGIN CERTIFICATE-----\n");
  EXPECT_EQ(c.verify_mode, "peer");
  EXPECT_EQ(c.source, "kubeconfig " + cfg.path.string());
}

TEST(KubeSettings, KubeconfigNamedContextWithClientCertificate) {
  const temp_file cfg(KUBECONFIG);
  kube_checks::settings s;
  s.kubeconfig = cfg.path.string();
  s.context = "lab";
  kube_checks::cluster c;
  std::string error;
  ASSERT_TRUE(kube_checks::resolve_cluster(s, c, error)) << error;
  EXPECT_EQ(c.host, "lab.example.com");
  EXPECT_EQ(c.port, "443");
  EXPECT_TRUE(c.token.empty());
  EXPECT_EQ(c.client_cert_pem, "CERT");
  EXPECT_EQ(c.client_key_pem, "KEY");
  EXPECT_EQ(c.ca_pem, "-----BEGIN CERTIFICATE-----\n");
  EXPECT_EQ(c.verify_mode, "peer");
}

TEST(KubeSettings, InsecureSkipTlsVerifyIsHonouredForATokenAndRefusedWithAClientCertificate) {
  const temp_file cfg(KUBECONFIG);
  kube_checks::settings s;
  s.kubeconfig = cfg.path.string();
  kube_checks::cluster c;
  std::string error;

  s.context = "insecure";
  ASSERT_TRUE(kube_checks::resolve_cluster(s, c, error)) << error;
  EXPECT_EQ(c.host, "insecure.example.com");
  EXPECT_EQ(c.token, "prod-token");
  EXPECT_EQ(c.verify_mode, "none") << "insecure-skip-tls-verify maps to verify mode none";

  // A client certificate on an unverified server can never connect (the HTTP
  // client refuses mTLS without server authentication), so it is rejected
  // up front with the fix.
  s.context = "insecure-cert";
  EXPECT_FALSE(kube_checks::resolve_cluster(s, c, error));
  EXPECT_NE(error.find("insecure-skip-tls-verify cannot be combined with a client certificate"), std::string::npos) << error;
  EXPECT_NE(error.find("Remove insecure-skip-tls-verify and supply certificate-authority(-data)"), std::string::npos) << error;

  // CA data pins the server and the client verifies against a pin whatever
  // the verify mode says, so the flag would be silently ignored; client-go
  // refuses the pair and so does this.
  s.context = "insecure-ca";
  EXPECT_FALSE(kube_checks::resolve_cluster(s, c, error));
  EXPECT_NE(error.find("insecure-skip-tls-verify cannot be combined with certificate-authority-data"), std::string::npos) << error;
}

TEST(KubeSettings, VerifyModeNoneIsRefusedWithKubeconfigCaData) {
  // The settings-level twin of insecure-skip-tls-verify: the CA data is a pin
  // and the client verifies against a pin whatever the verify mode says, so
  // `verify mode = none` would be silently ignored.
  const temp_file cfg(KUBECONFIG);
  kube_checks::settings s;
  s.kubeconfig = cfg.path.string();
  kube_checks::cluster c;
  std::string error;
  for (const std::string &mode : {"none", ""}) {
    s.verify_mode = mode;
    EXPECT_FALSE(kube_checks::resolve_cluster(s, c, error)) << "verify mode '" << mode << "'";
    EXPECT_NE(error.find("cannot be combined with certificate-authority-data"), std::string::npos) << error;
  }

  // Without CA data there is no pin, so `none` does what it says.
  s.verify_mode = "none";
  s.context = "insecure";
  ASSERT_TRUE(kube_checks::resolve_cluster(s, c, error)) << error;
  EXPECT_EQ(c.verify_mode, "none");

  // And a verifying mode with CA data is the ordinary case.
  s.verify_mode = "peer";
  s.context.clear();
  ASSERT_TRUE(kube_checks::resolve_cluster(s, c, error)) << error;
}

TEST(KubeSettings, KubeconfigFileReferencesResolveAgainstItsOwnDirectory) {
  // client-go resolves certificate-authority, client-certificate, client-key
  // and tokenFile relative to the kubeconfig's directory, not the process
  // cwd; a kubeconfig that works with kubectl must work here.
  const temp_file token("relative-token\n");
  const temp_file cert("CERT-FROM-FILE");
  const temp_file key("KEY-FROM-FILE");
  const temp_file ca("CA-FROM-FILE");
  const std::string token_name = token.path.filename().string();
  const std::string cert_name = cert.path.filename().string();
  const std::string key_name = key.path.filename().string();
  const std::string ca_name = ca.path.filename().string();
  const temp_file cfg(R"({"current-context":"c",
    "clusters":[{"name":"x","cluster":{"server":"https://x.example.com:6443","certificate-authority":")" +
                      ca_name + R"("}}],
    "contexts":[{"name":"c","context":{"cluster":"x","user":"tok"}},{"name":"mtls","context":{"cluster":"x","user":"cert"}}],
    "users":[{"name":"tok","user":{"tokenFile":")" +
                      token_name + R"("}},
             {"name":"cert","user":{"client-certificate":")" +
                      cert_name + R"(","client-key":")" + key_name + R"("}}]})");
  kube_checks::settings s;
  s.kubeconfig = cfg.path.string();
  kube_checks::cluster c;
  std::string error;
  ASSERT_TRUE(kube_checks::resolve_cluster(s, c, error)) << error;
  EXPECT_EQ(c.token, "relative-token");
  EXPECT_EQ(c.ca, ca.path.string());

  s.context = "mtls";
  ASSERT_TRUE(kube_checks::resolve_cluster(s, c, error)) << error;
  EXPECT_EQ(c.client_cert_pem, "CERT-FROM-FILE");
  EXPECT_EQ(c.client_key_pem, "KEY-FROM-FILE");

  // An absolute or rooted path is left alone, on Windows too, where a path
  // without a drive letter is rooted but not absolute.
  EXPECT_EQ(kube_checks::detail::resolve_relative("/etc/nscp", "/abs/ca.pem"), "/abs/ca.pem");
  EXPECT_EQ(kube_checks::detail::resolve_relative("C:\\nscp", "/abs/ca.pem"), "/abs/ca.pem");
#ifdef _WIN32
  EXPECT_EQ(kube_checks::detail::resolve_relative("C:\\nscp", "C:\\certs\\ca.pem"), "C:\\certs\\ca.pem");
#endif
  EXPECT_EQ(kube_checks::detail::resolve_relative("/etc/nscp", ""), "");
  EXPECT_EQ(kube_checks::detail::resolve_relative("", "ca.pem"), "ca.pem");
}

TEST(KubeSettings, KubeconfigErrorsAreSpecific) {
  const temp_file cfg(KUBECONFIG);
  kube_checks::settings s;
  s.kubeconfig = cfg.path.string();
  kube_checks::cluster c;
  std::string error;

  s.context = "nope";
  EXPECT_FALSE(kube_checks::resolve_cluster(s, c, error));
  EXPECT_NE(error.find("no context named 'nope'"), std::string::npos) << error;

  s.context = "cloud";
  EXPECT_FALSE(kube_checks::resolve_cluster(s, c, error));
  EXPECT_NE(error.find("exec and auth-provider credentials are not supported"), std::string::npos) << error;

  const temp_file yaml("apiVersion: v1\nkind: Config\nclusters:\n- name: x\n");
  s.context.clear();
  s.kubeconfig = yaml.path.string();
  EXPECT_FALSE(kube_checks::resolve_cluster(s, c, error));
  EXPECT_NE(error.find("YAML is not supported"), std::string::npos) << error;

  s.kubeconfig = "/nonexistent/nscp/kubeconfig.json";
  EXPECT_FALSE(kube_checks::resolve_cluster(s, c, error));
  EXPECT_NE(error.find("Failed to read kubeconfig"), std::string::npos) << error;
}

TEST(KubeSettings, InClusterWithoutAMountedTokenIsExplained) {
  const scoped_env host("KUBERNETES_SERVICE_HOST", "10.96.0.1");
  const scoped_env port("KUBERNETES_SERVICE_PORT", "443");
  kube_checks::settings s;
  kube_checks::cluster c;
  std::string error;
  if (boost::filesystem::exists(kube_checks::in_cluster_token_path())) {
    // The test itself runs in a pod: the projected token is picked up.
    ASSERT_TRUE(kube_checks::resolve_cluster(s, c, error)) << error;
    EXPECT_EQ(c.host, "10.96.0.1");
    EXPECT_EQ(c.source, "in-cluster");
    return;
  }
  EXPECT_FALSE(kube_checks::resolve_cluster(s, c, error));
  EXPECT_NE(error.find("Running in-cluster"), std::string::npos) << error;
  EXPECT_NE(error.find("service account token"), std::string::npos) << error;
}

TEST(KubeSettings, InClusterHonoursAConfiguredToken) {
  // `token` / `token file` without `api server`: the server is auto-detected
  // but the credential the operator configured is the one that is sent, not
  // the mounted service account's.
  const scoped_env host("KUBERNETES_SERVICE_HOST", "10.96.0.1");
  kube_checks::settings s;
  s.token = "configured-token";
  kube_checks::cluster c;
  std::string error;
  ASSERT_TRUE(kube_checks::resolve_cluster(s, c, error)) << error;
  EXPECT_EQ(c.host, "10.96.0.1");
  EXPECT_EQ(c.source, "in-cluster");
  EXPECT_EQ(c.token, "configured-token");

  const temp_file token("projected-token\n");
  s.token_file = token.path.string();
  ASSERT_TRUE(kube_checks::resolve_cluster(s, c, error)) << error;
  EXPECT_EQ(c.token, "projected-token") << "the token file wins over the inline token here too";

  s.token_file = "/nonexistent/nscp/token";
  EXPECT_FALSE(kube_checks::resolve_cluster(s, c, error));
  EXPECT_NE(error.find("Invalid `token file`"), std::string::npos) << error;
}

TEST(KubeSettings, InClusterKeepsAConfiguredCa) {
  // The mounted service account CA stands in for the default bundle only: a
  // `ca` the operator set (one that also trusts a TLS-intercepting mesh) is
  // the one used, as a configured token is.
  const scoped_env host("KUBERNETES_SERVICE_HOST", "10.96.0.1");
  kube_checks::settings s;
  s.token = "configured-token";
  s.default_ca = "/etc/ssl/certs";
  s.ca = "/etc/nscp/mesh-ca.pem";
  kube_checks::cluster c;
  std::string error;
  ASSERT_TRUE(kube_checks::resolve_cluster(s, c, error)) << error;
  EXPECT_EQ(c.ca, "/etc/nscp/mesh-ca.pem");

  // Left at the default, the mounted CA replaces it when there is one.
  s.ca = s.default_ca;
  ASSERT_TRUE(kube_checks::resolve_cluster(s, c, error)) << error;
  const bool mounted = boost::filesystem::exists(kube_checks::in_cluster_ca_path());
  EXPECT_EQ(c.ca, mounted ? std::string(kube_checks::in_cluster_ca_path()) : s.default_ca);
}

TEST(KubeSettings, ExplicitServerWinsOverKubeconfigAndEnvironment) {
  const scoped_env host("KUBERNETES_SERVICE_HOST", "10.96.0.1");
  const temp_file cfg(KUBECONFIG);
  kube_checks::settings s = configured();
  s.kubeconfig = cfg.path.string();
  kube_checks::cluster c;
  std::string error;
  ASSERT_TRUE(kube_checks::resolve_cluster(s, c, error)) << error;
  EXPECT_EQ(c.host, "k8s.example.com");
  EXPECT_EQ(c.source, "settings");
}

// --- client helpers --------------------------------------------------------------

TEST(KubeClient, ListQueryEncodesSelectors) {
  kube_checks::list_options opt;
  opt.label_selector = "app=web,tier in (a,b)";
  opt.field_selector = "status.phase!=Succeeded";
  EXPECT_EQ(kube_checks::list_query(opt, ""), "?limit=500&labelSelector=app%3Dweb%2Ctier%20in%20%28a%2Cb%29&fieldSelector=status.phase%21%3DSucceeded");
  EXPECT_EQ(kube_checks::list_query(kube_checks::list_options(), "tok"), "?limit=500&continue=tok");
  EXPECT_EQ(kube_checks::list_path("/api/v1", "", "pods"), "/api/v1/pods");
  EXPECT_EQ(kube_checks::list_path("/apis/apps/v1", "kube-system", "deployments"), "/apis/apps/v1/namespaces/kube-system/deployments");
}

TEST(KubeClient, RequiredNamesMatchBareOrQualifiedAndReportTheRest) {
  kube_checks::required_names wanted({"web", "ops/ghost", "db-0"});
  EXPECT_FALSE(wanted.empty());
  EXPECT_TRUE(wanted.claim("web", "shop/web"));
  EXPECT_TRUE(wanted.claim("ghost", "ops/ghost"));
  EXPECT_FALSE(wanted.claim("ghost", "shop/ghost")) << "a qualified name matches its own namespace only";
  EXPECT_FALSE(wanted.claim("other", "shop/other"));
  std::vector<std::string> missing;
  wanted.for_each_missing("*", [&missing](const std::string &ns, const std::string &name) { missing.push_back(ns + "/" + name); });
  EXPECT_EQ(missing, std::vector<std::string>({"*/db-0"}));

  kube_checks::required_names none({});
  EXPECT_TRUE(none.empty());
  EXPECT_EQ(kube_checks::default_namespace({}), "*");
  EXPECT_EQ(kube_checks::default_namespace({"ops"}), "ops");
  EXPECT_EQ(kube_checks::default_namespace({"ops", "shop"}), "*");
}

// --- check_pods -------------------------------------------------------------------

namespace {

// Five pods as the API server reports them, trimmed to what the check reads
// plus fields it must ignore: a healthy web pod, a crash-looping api pod, a
// finished job, a pod being deleted and one that cannot be scheduled.
const char *FIVE_PODS = R"json({"kind":"PodList","metadata":{"resourceVersion":"1"},"items":[
  {"metadata":{"name":"web-7d4b9c-abcde","namespace":"default","creationTimestamp":"2020-01-01T00:00:00Z",
               "labels":{"app":"web","tier":"frontend"},
               "ownerReferences":[{"kind":"ReplicaSet","name":"web-7d4b9c","controller":true}]},
   "spec":{"nodeName":"worker-1","containers":[{"name":"nginx"}]},
   "status":{"phase":"Running","podIP":"10.244.1.5","qosClass":"Burstable",
             "conditions":[{"type":"Ready","status":"True"}],
             "containerStatuses":[{"name":"nginx","ready":true,"restartCount":0,"state":{"running":{}}}]}},
  {"metadata":{"name":"api-5f6c7-xyz12","namespace":"default","creationTimestamp":"2026-09-25T00:00:00Z",
               "ownerReferences":[{"kind":"ReplicaSet","name":"api-5f6c7","controller":true}]},
   "spec":{"nodeName":"worker-2","containers":[{"name":"api"}]},
   "status":{"phase":"Running","podIP":"10.244.2.9","qosClass":"BestEffort",
             "conditions":[{"type":"Ready","status":"False"}],
             "containerStatuses":[{"name":"api","ready":false,"restartCount":7,
                                   "state":{"waiting":{"reason":"CrashLoopBackOff"}},
                                   "lastState":{"terminated":{"exitCode":1,"reason":"Error"}}}]}},
  {"metadata":{"name":"backup-28800-q9x","namespace":"ops","creationTimestamp":"2026-09-24T00:00:00Z",
               "ownerReferences":[{"kind":"Job","name":"backup-28800","controller":true}]},
   "spec":{"nodeName":"worker-1","containers":[{"name":"backup"}]},
   "status":{"phase":"Succeeded","qosClass":"BestEffort",
             "containerStatuses":[{"name":"backup","ready":false,"restartCount":0,"state":{"terminated":{"exitCode":0,"reason":"Completed"}}}]}},
  {"metadata":{"name":"old-1","namespace":"default","deletionTimestamp":"2026-09-25T10:00:00Z"},
   "spec":{"nodeName":"worker-1","containers":[{"name":"old"}]},
   "status":{"phase":"Running","containerStatuses":[{"name":"old","ready":true,"restartCount":0,"state":{"running":{}}}]}},
  {"metadata":{"name":"big-0","namespace":"default","ownerReferences":[{"kind":"StatefulSet","name":"big","controller":true}]},
   "spec":{"containers":[{"name":"big"}]},
   "status":{"phase":"Pending","conditions":[{"type":"PodScheduled","status":"False","reason":"Unschedulable"}]}}
]})json";

const char *TWO_HEALTHY_PODS = R"json({"items":[
  {"metadata":{"name":"a","namespace":"default"},"spec":{"containers":[{"name":"a"}]},
   "status":{"phase":"Running","conditions":[{"type":"Ready","status":"True"}],"containerStatuses":[{"name":"a","ready":true,"state":{"running":{}}}]}},
  {"metadata":{"name":"b","namespace":"default"},"spec":{"containers":[{"name":"b"}]},
   "status":{"phase":"Running","conditions":[{"type":"Ready","status":"True"}],"containerStatuses":[{"name":"b","ready":true,"state":{"running":{}}}]}}
]})json";

}  // namespace

TEST(CheckPods, DefaultThresholdsFlagTheCrashLoopAndHideFinishedJobs) {
  fake_api api;
  api.serve("/api/v1/pods", FIVE_PODS);
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_pods(api.factory(), {}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  const std::string msg = join_lines(response);
  EXPECT_EQ(msg, "CRITICAL: default/api-5f6c7-xyz12=CrashLoopBackOff, default/big-0=Pending") << "the crash loop is critical, the pending pod a warning";
  EXPECT_EQ(msg.find("backup"), std::string::npos) << "Succeeded pods are filtered out by default";
  EXPECT_EQ(api.requests.size(), 1u);
  EXPECT_EQ(api.requests[0], "/api/v1/pods?limit=500");
}

TEST(CheckPods, HealthyPodsAreOkWithACount) {
  fake_api api;
  api.serve("/api/v1/pods", TWO_HEALTHY_PODS);
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_pods(api.factory(), {}, response), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_EQ(join_lines(response), "OK: All 2 pods are fine");
}

TEST(CheckPods, NoPodsIsTheDocumentedOk) {
  fake_api api;
  api.serve("/api/v1/pods", R"({"items":[]})");
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_pods(api.factory(), {}, response), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_EQ(join_lines(response), "OK: No pods found");
}

TEST(CheckPods, KeywordsAreExposed) {
  fake_api api;
  api.serve("/api/v1/pods", FIVE_PODS);
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_pods(api.factory(),
                     {"filter=name like 'web'",
                      "detail-syntax=%(namespace)/%(name)|%(node)|%(phase)|%(pod_status)|%(ready_containers)/"
                      "%(containers)|%(restarts)|%(owner_kind)|%(owner)|%(qos)|%(ip)|%(labels)|%(ready)|%(terminating)|%(oom_killed)",
                      "top-syntax=${list}", "ok-syntax="},
                     response),
            PB::Common::ResultCode::OK)
      << join_lines(response);
  EXPECT_EQ(join_lines(response),
            "default/web-7d4b9c-abcde|worker-1|Running|Running|1/1|0|ReplicaSet|web-7d4b9c|Burstable|10.244.1.5|app=web,tier=frontend|1|0|0");
}

TEST(CheckPods, TerminatingAndStatusKeywords) {
  fake_api api;
  api.serve("/api/v1/pods", FIVE_PODS);
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_pods(api.factory(), {"filter=terminating = 1", "detail-syntax=%(name)=%(pod_status)", "top-syntax=${list}", "ok-syntax="}, response),
            PB::Common::ResultCode::OK)
      << join_lines(response);
  EXPECT_EQ(join_lines(response), "old-1=Terminating");
}

TEST(CheckPods, AgeTakesUnitsAndCreatedIsADate) {
  fake_api api;
  api.serve("/api/v1/pods", FIVE_PODS);
  PB::Commands::QueryResponseMessage::Response response;
  // The web pod was created in 2020, the api pod today (fixture date); a
  // pod younger than a day is what a restart storm looks like.
  EXPECT_EQ(run_pods(api.factory(), {"filter=name like 'web'", "critical=age < 1d", "warning=age < 1d"}, response), PB::Common::ResultCode::OK)
      << join_lines(response);
  PB::Commands::QueryResponseMessage::Response response2;
  EXPECT_EQ(run_pods(api.factory(), {"filter=name like 'web'", "critical=age > 365d", "warning=none"}, response2), PB::Common::ResultCode::CRITICAL)
      << join_lines(response2);
  PB::Commands::QueryResponseMessage::Response response3;
  EXPECT_EQ(run_pods(api.factory(), {"filter=name like 'web'", "critical=created > -10d", "warning=none"}, response3), PB::Common::ResultCode::OK)
      << join_lines(response3);
}

TEST(CheckPods, RestartsThresholdEmitsPerfData) {
  fake_api api;
  api.serve("/api/v1/pods", FIVE_PODS);
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_pods(api.factory(), {"filter=namespace = 'default'", "warning=restarts > 5", "critical=none"}, response), PB::Common::ResultCode::WARNING)
      << join_lines(response);
  EXPECT_EQ(join_lines(response), "WARNING: default/api-5f6c7-xyz12=CrashLoopBackOff");
  EXPECT_NE(perf_of(response).find("default/api-5f6c7-xyz12 restarts=7"), std::string::npos) << perf_of(response);
}

TEST(CheckPods, NamespaceAndSelectorsReachTheServer) {
  fake_api api;
  api.serve("/api/v1/namespaces/kube-system/pods", TWO_HEALTHY_PODS);
  api.serve("/api/v1/namespaces/monitoring/pods", R"({"items":[]})");
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(
      run_pods(api.factory(), {"namespace=kube-system", "namespace=monitoring", "label-selector=app=web,tier!=cache", "field-selector=spec.nodeName=worker-1"},
               response),
      PB::Common::ResultCode::OK)
      << join_lines(response);
  ASSERT_EQ(api.requests.size(), 2u);
  EXPECT_EQ(api.requests[0], "/api/v1/namespaces/kube-system/pods?limit=500&labelSelector=app%3Dweb%2Ctier%21%3Dcache&fieldSelector=spec.nodeName%3Dworker-1");
  EXPECT_EQ(api.requests[1], "/api/v1/namespaces/monitoring/pods?limit=500&labelSelector=app%3Dweb%2Ctier%21%3Dcache&fieldSelector=spec.nodeName%3Dworker-1");
  EXPECT_EQ(join_lines(response), "OK: All 2 pods are fine");
}

TEST(CheckPods, PodListIsPaginated) {
  fake_api api;
  api.serve("/api/v1/pods?limit=500", R"({"metadata":{"continue":"ENCODED"},"items":[
    {"metadata":{"name":"a","namespace":"default"},"spec":{"containers":[{"name":"a"}]},"status":{"phase":"Running","conditions":[{"type":"Ready","status":"True"}],"containerStatuses":[{"name":"a","ready":true,"state":{"running":{}}}]}}]})");
  api.serve("/api/v1/pods?limit=500&continue=ENCODED", R"({"metadata":{},"items":[
    {"metadata":{"name":"b","namespace":"default"},"spec":{"containers":[{"name":"b"}]},"status":{"phase":"Running","conditions":[{"type":"Ready","status":"True"}],"containerStatuses":[{"name":"b","ready":true,"state":{"running":{}}}]}},
    {"metadata":{"name":"c","namespace":"default"},"spec":{"containers":[{"name":"c"}]},"status":{"phase":"Failed"}}]})");
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_pods(api.factory(), {}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  EXPECT_EQ(join_lines(response), "CRITICAL: default/c=Failed");
  ASSERT_EQ(api.requests.size(), 2u);
  EXPECT_EQ(api.requests[1], "/api/v1/pods?limit=500&continue=ENCODED");
}

TEST(CheckPods, RequiredPodsAndMissingOnes) {
  fake_api api;
  api.serve("/api/v1/pods", FIVE_PODS);
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_pods(api.factory(), {"pod=web-7d4b9c-abcde"}, response), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_EQ(join_lines(response), "OK: All 1 pods are fine");

  PB::Commands::QueryResponseMessage::Response response2;
  EXPECT_EQ(run_pods(api.factory(), {"pod=web-7d4b9c-abcde", "pod=ops/ghost", "pod=nowhere"}, response2), PB::Common::ResultCode::CRITICAL)
      << join_lines(response2);
  EXPECT_EQ(join_lines(response2), "CRITICAL: ops/ghost=missing, */nowhere=missing") << "a bare name looked up everywhere is reported under *";

  PB::Commands::QueryResponseMessage::Response response3;
  api.serve("/api/v1/namespaces/ops/pods", R"({"items":[]})");
  EXPECT_EQ(run_pods(api.factory(), {"namespace=ops", "pod=ghost", "detail-syntax=%(namespace)/%(name)=%(phase)"}, response3), PB::Common::ResultCode::CRITICAL)
      << join_lines(response3);
  EXPECT_EQ(join_lines(response3), "CRITICAL: ops/ghost=missing") << "a single namespace= names the missing pod's namespace";
}

TEST(CheckPods, ForbiddenNamesTheNamespace) {
  fake_api api;
  api.refuse(
      "/api/v1/namespaces/secret/pods", 403,
      R"({"kind":"Status","message":"pods is forbidden: User \"system:serviceaccount:monitoring:nscp\" cannot list resource \"pods\" in API group \"\" in the namespace \"secret\"","reason":"Forbidden","code":403})");
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_pods(api.factory(), {"namespace=secret"}, response), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
  EXPECT_NE(join_lines(response).find("in the namespace \"secret\""), std::string::npos) << join_lines(response);
  EXPECT_EQ(join_lines(response).find(TOKEN), std::string::npos);
}

// --- check_nodes ------------------------------------------------------------------

namespace {

// Three nodes: a healthy control plane, a worker under disk pressure that
// has gone NotReady, and a cordoned worker.
const char *THREE_FULL_NODES = R"json({"kind":"NodeList","metadata":{},"items":[
  {"metadata":{"name":"cp-1","creationTimestamp":"2024-01-01T00:00:00Z",
               "labels":{"node-role.kubernetes.io/control-plane":"","kubernetes.io/hostname":"cp-1"}},
   "spec":{"taints":[{"key":"node-role.kubernetes.io/control-plane","effect":"NoSchedule"}]},
   "status":{"conditions":[{"type":"MemoryPressure","status":"False"},{"type":"DiskPressure","status":"False"},
                           {"type":"PIDPressure","status":"False"},{"type":"Ready","status":"True"}],
             "addresses":[{"type":"InternalIP","address":"10.0.0.10"},{"type":"Hostname","address":"cp-1"}],
             "nodeInfo":{"kubeletVersion":"v1.30.2","osImage":"Ubuntu 22.04.4 LTS","architecture":"amd64"},
             "capacity":{"cpu":"4","memory":"16386764Ki","pods":"110"},
             "allocatable":{"cpu":"3800m","memory":"15761100Ki","pods":"110"}}},
  {"metadata":{"name":"worker-1","creationTimestamp":"2024-01-02T00:00:00Z","labels":{"node-role.kubernetes.io/worker":""}},
   "spec":{},
   "status":{"conditions":[{"type":"MemoryPressure","status":"False"},{"type":"DiskPressure","status":"True"},
                           {"type":"PIDPressure","status":"False"},{"type":"Ready","status":"False"}],
             "nodeInfo":{"kubeletVersion":"v1.30.2","osImage":"Ubuntu 22.04.4 LTS","architecture":"amd64"},
             "capacity":{"cpu":"8","memory":"32Gi","pods":"110"},
             "allocatable":{"cpu":"7900m","memory":"31Gi","pods":"110"}}},
  {"metadata":{"name":"worker-2","creationTimestamp":"2024-01-03T00:00:00Z"},
   "spec":{"unschedulable":true,"taints":[{"key":"node.kubernetes.io/unschedulable","effect":"NoSchedule"},{"key":"dedicated","value":"gpu","effect":"NoExecute"}]},
   "status":{"conditions":[{"type":"MemoryPressure","status":"False"},{"type":"DiskPressure","status":"False"},
                           {"type":"PIDPressure","status":"False"},{"type":"Ready","status":"True"}],
             "nodeInfo":{"kubeletVersion":"v1.29.8","osImage":"Debian GNU/Linux 12 (bookworm)","architecture":"arm64"},
             "capacity":{"cpu":"8","memory":"32Gi","pods":"110"},
             "allocatable":{"cpu":"7900m","memory":"31Gi","pods":"110"}}}
]})json";

}  // namespace

TEST(CheckNodes, DefaultThresholdsFlagNotReadyAndCordoned) {
  fake_api api;
  api.serve("/api/v1/nodes", THREE_FULL_NODES);
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_nodes(api.factory(), {}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  EXPECT_EQ(join_lines(response), "CRITICAL: worker-1=NotReady, worker-2=Ready,SchedulingDisabled");
  EXPECT_EQ(api.requests[0], "/api/v1/nodes?limit=500");
}

TEST(CheckNodes, HealthyNodesAreOkWithACount) {
  fake_api api;
  api.serve("/api/v1/nodes", THREE_NODES.substr(0, THREE_NODES.find(",{\"metadata\":{\"name\":\"worker-2\"")) + "]}");
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_nodes(api.factory(), {}, response), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_EQ(join_lines(response), "OK: All 2 nodes are ready");
}

TEST(CheckNodes, KeywordsAreExposed) {
  fake_api api;
  api.serve("/api/v1/nodes", THREE_FULL_NODES);
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_nodes(api.factory(),
                      {"filter=name = 'cp-1'",
                       "detail-syntax=%(name)|%(ready)|%(node_status)|%(schedulable)|%(roles)|%(taints)|%(kubelet_version)|%(os)|%(arch)|%(internal_ip)|%(cpu_"
                       "capacity)|%(cpu_allocatable)|%(memory_capacity)|%(memory_allocatable)|%(pods_capacity)|%(disk_pressure)",
                       "top-syntax=${list}", "ok-syntax="},
                      response),
            PB::Common::ResultCode::OK)
      << join_lines(response);
  EXPECT_EQ(join_lines(response),
            "cp-1|True|Ready|1|control-plane|node-role.kubernetes.io/control-plane:NoSchedule|v1.30.2|Ubuntu 22.04.4 "
            "LTS|amd64|10.0.0.10|4000|3800|16780046336|16139366400|110|0");
}

TEST(CheckNodes, PressureAndTaintKeywords) {
  fake_api api;
  api.serve("/api/v1/nodes", THREE_FULL_NODES);
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_nodes(api.factory(),
                      {"filter=disk_pressure = 1 or taints like 'dedicated'", "detail-syntax=%(name)=%(disk_pressure)/%(taints)", "top-syntax=${list}",
                       "ok-syntax=", "warning=none", "critical=none"},
                      response),
            PB::Common::ResultCode::OK)
      << join_lines(response);
  EXPECT_EQ(join_lines(response), "worker-1=1/, worker-2=0/node.kubernetes.io/unschedulable:NoSchedule,dedicated=gpu:NoExecute");
}

TEST(CheckNodes, MemoryThresholdsTakeUnitsAndEmitPerf) {
  fake_api api;
  api.serve("/api/v1/nodes", THREE_FULL_NODES);
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_nodes(api.factory(), {"filter=name = 'worker-2'", "warning=memory_allocatable < 40G", "critical=cpu_allocatable < 1000"}, response),
            PB::Common::ResultCode::WARNING)
      << join_lines(response);
  EXPECT_EQ(join_lines(response), "WARNING: worker-2=Ready,SchedulingDisabled");
  EXPECT_NE(perf_of(response).find("worker-2 memory allocatable=33285996544"), std::string::npos) << perf_of(response);
  EXPECT_NE(perf_of(response).find("worker-2 cpu allocatable=7900"), std::string::npos) << perf_of(response);
}

TEST(CheckNodes, RequiredNodesAndMissingOnes) {
  fake_api api;
  api.serve("/api/v1/nodes", THREE_FULL_NODES);
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_nodes(api.factory(), {"node=cp-1", "node=gone-1"}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  EXPECT_EQ(join_lines(response), "CRITICAL: gone-1=missing");
  PB::Commands::QueryResponseMessage::Response response2;
  EXPECT_EQ(run_nodes(api.factory(), {"node=cp-1"}, response2), PB::Common::ResultCode::OK) << join_lines(response2);
  EXPECT_EQ(join_lines(response2), "OK: All 1 nodes are ready");
}

TEST(CheckNodes, SelectorsReachTheServer) {
  fake_api api;
  api.serve("/api/v1/nodes", R"({"items":[]})");
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_nodes(api.factory(), {"label-selector=node-role.kubernetes.io/worker="}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  EXPECT_EQ(join_lines(response), "CRITICAL: No nodes found") << "an empty node list is the documented critical";
  EXPECT_EQ(api.requests[0], "/api/v1/nodes?limit=500&labelSelector=node-role.kubernetes.io%2Fworker%3D");
}

// --- check_workloads --------------------------------------------------------------

namespace {

const char *DEPLOYMENTS = R"json({"items":[
  {"metadata":{"name":"web","namespace":"shop","creationTimestamp":"2025-01-01T00:00:00Z","labels":{"app":"web"}},
   "spec":{"replicas":3},
   "status":{"replicas":3,"readyReplicas":3,"availableReplicas":3,"updatedReplicas":3}},
  {"metadata":{"name":"api","namespace":"shop"},
   "spec":{"replicas":3},
   "status":{"replicas":3,"readyReplicas":1,"availableReplicas":1,"updatedReplicas":3,"unavailableReplicas":2}},
  {"metadata":{"name":"worker","namespace":"shop"},
   "spec":{"replicas":2,"paused":true},
   "status":{"replicas":2,"readyReplicas":2,"availableReplicas":2,"updatedReplicas":1}},
  {"metadata":{"name":"legacy","namespace":"ops"},
   "spec":{"replicas":2},
   "status":{"replicas":2,"unavailableReplicas":2}},
  {"metadata":{"name":"scaled-down","namespace":"ops"},
   "spec":{"replicas":0},
   "status":{}}
]})json";

const char *STATEFULSETS = R"json({"items":[
  {"metadata":{"name":"db","namespace":"shop"},
   "spec":{"replicas":3},
   "status":{"replicas":3,"readyReplicas":3,"availableReplicas":3,"updatedReplicas":3,"currentReplicas":3}},
  {"metadata":{"name":"old-db","namespace":"ops"},
   "spec":{},
   "status":{"replicas":1,"readyReplicas":1,"updatedReplicas":1}}
]})json";

const char *DAEMONSETS = R"json({"items":[
  {"metadata":{"name":"node-exporter","namespace":"monitoring"},
   "spec":{},
   "status":{"desiredNumberScheduled":3,"currentNumberScheduled":3,"numberReady":2,"numberAvailable":2,"updatedNumberScheduled":3,"numberUnavailable":1,"numberMisscheduled":0}}
]})json";

void serve_all_kinds(fake_api &api) {
  api.serve("/apis/apps/v1/deployments", DEPLOYMENTS);
  api.serve("/apis/apps/v1/statefulsets", STATEFULSETS);
  api.serve("/apis/apps/v1/daemonsets", DAEMONSETS);
}

}  // namespace

TEST(CheckWorkloads, DefaultThresholdsAcrossAllKinds) {
  fake_api api;
  serve_all_kinds(api);
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_workloads(api.factory(), {}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  // api is short (warning), worker's rollout is behind (warning), legacy has
  // nothing available (critical), the daemonset misses a pod (warning); the
  // scaled-down deployment wants nothing and is fine.
  EXPECT_EQ(join_lines(response),
            "CRITICAL: Deployment shop/api=1/3, Deployment shop/worker=2/2, Deployment ops/legacy=0/2, DaemonSet monitoring/node-exporter=2/3");
  ASSERT_EQ(api.requests.size(), 3u);
  EXPECT_EQ(api.requests[0], "/apis/apps/v1/deployments?limit=500");
  EXPECT_EQ(api.requests[1], "/apis/apps/v1/statefulsets?limit=500");
  EXPECT_EQ(api.requests[2], "/apis/apps/v1/daemonsets?limit=500");
}

TEST(CheckWorkloads, KindRestrictsTheCallsAndAcceptsPluralsAndCase) {
  fake_api api;
  serve_all_kinds(api);
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_workloads(api.factory(), {"kind=DaemonSets"}, response), PB::Common::ResultCode::WARNING) << join_lines(response);
  EXPECT_EQ(join_lines(response), "WARNING: DaemonSet monitoring/node-exporter=2/3");
  ASSERT_EQ(api.requests.size(), 1u);
  EXPECT_EQ(api.requests[0], "/apis/apps/v1/daemonsets?limit=500");

  PB::Commands::QueryResponseMessage::Response response2;
  EXPECT_EQ(run_workloads(api.factory(), {"kind=statefulset", "kind=deployment"}, response2), PB::Common::ResultCode::CRITICAL) << join_lines(response2);
  EXPECT_EQ(api.requests.size(), 3u);

  PB::Commands::QueryResponseMessage::Response response3;
  EXPECT_EQ(run_workloads(api.factory(), {"kind=cronjob"}, response3), PB::Common::ResultCode::UNKNOWN) << join_lines(response3);
  EXPECT_NE(join_lines(response3).find("Unknown workload kind 'cronjob'"), std::string::npos) << join_lines(response3);
}

TEST(CheckWorkloads, KeywordsAreExposed) {
  fake_api api;
  serve_all_kinds(api);
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_workloads(api.factory(),
                          {"filter=namespace = 'shop'", "warning=none", "critical=none",
                           "detail-syntax=%(kind)/%(name)|%(desired)|%(ready)|%(available)|%(updated)|%(unavailable)|%(missing)|%(paused)|%(labels)",
                           "top-syntax=${list}", "ok-syntax="},
                          response),
            PB::Common::ResultCode::OK)
      << join_lines(response);
  EXPECT_EQ(join_lines(response),
            "Deployment/web|3|3|3|3|0|0|0|app=web, Deployment/api|3|1|1|3|2|2|0|, Deployment/worker|2|2|2|1|0|0|1|, StatefulSet/db|3|3|3|3|0|0|0|");
}

TEST(CheckWorkloads, StatefulSetWithoutAvailableReplicasFallsBackToReady) {
  fake_api api;
  api.serve("/apis/apps/v1/statefulsets", STATEFULSETS);
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_workloads(api.factory(),
                          {"kind=statefulset", "filter=name = 'old-db'", "detail-syntax=%(name)=%(available)/%(desired)", "top-syntax=${list}", "ok-syntax="},
                          response),
            PB::Common::ResultCode::OK)
      << join_lines(response);
  EXPECT_EQ(join_lines(response), "old-db=1/1") << "spec.replicas defaults to 1 and available falls back to ready";
}

TEST(CheckWorkloads, PausedAndRolloutKeywordsInThresholds) {
  fake_api api;
  serve_all_kinds(api);
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_workloads(api.factory(), {"kind=deployment", "warning=paused = 1 and updated < 2", "critical=none"}, response), PB::Common::ResultCode::WARNING)
      << join_lines(response);
  EXPECT_EQ(join_lines(response), "WARNING: Deployment shop/worker=2/2");
  EXPECT_NE(perf_of(response).find("shop/worker updated=1"), std::string::npos) << perf_of(response);
}

TEST(CheckWorkloads, NamespaceAndSelectorsReachTheServer) {
  fake_api api;
  api.serve("/apis/apps/v1/namespaces/shop/deployments", R"({"items":[]})");
  api.serve("/apis/apps/v1/namespaces/shop/statefulsets", R"({"items":[]})");
  api.serve("/apis/apps/v1/namespaces/shop/daemonsets", R"({"items":[]})");
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_workloads(api.factory(), {"namespace=shop", "label-selector=app.kubernetes.io/part-of=shop"}, response), PB::Common::ResultCode::OK)
      << join_lines(response);
  EXPECT_EQ(join_lines(response), "OK: No workloads found");
  ASSERT_EQ(api.requests.size(), 3u);
  EXPECT_EQ(api.requests[0], "/apis/apps/v1/namespaces/shop/deployments?limit=500&labelSelector=app.kubernetes.io%2Fpart-of%3Dshop");
}

TEST(CheckWorkloads, RequiredWorkloadsAndMissingOnes) {
  fake_api api;
  serve_all_kinds(api);
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_workloads(api.factory(), {"workload=shop/web", "workload=db"}, response), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_EQ(join_lines(response), "OK: All 2 workloads are available");

  PB::Commands::QueryResponseMessage::Response response2;
  EXPECT_EQ(run_workloads(api.factory(), {"workload=shop/web", "workload=shop/ghost"}, response2), PB::Common::ResultCode::CRITICAL) << join_lines(response2);
  EXPECT_EQ(join_lines(response2), "CRITICAL: missing shop/ghost=0/1");
}

TEST(CheckWorkloads, ForbiddenNamesTheResource) {
  fake_api api;
  api.serve("/apis/apps/v1/deployments", DEPLOYMENTS);
  api.refuse(
      "/apis/apps/v1/statefulsets", 403,
      R"({"kind":"Status","message":"statefulsets.apps is forbidden: User \"system:serviceaccount:monitoring:nscp\" cannot list resource \"statefulsets\" in API group \"apps\" at the cluster scope","reason":"Forbidden","code":403})");
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_workloads(api.factory(), {}, response), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
  EXPECT_NE(join_lines(response).find("cannot list resource \"statefulsets\" in API group \"apps\""), std::string::npos) << join_lines(response);
  EXPECT_EQ(response.lines_size(), 1) << "the partial deployment result is dropped, the error is the only line";
}

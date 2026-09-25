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
  EXPECT_NE(msg.find("grant the agent's service account get and list"), std::string::npos) << msg;
  EXPECT_EQ(msg.find(TOKEN), std::string::npos) << msg;
}

TEST(CheckKubernetes, UnreachableServerIsUnknown) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_cluster(unreachable_api("Failed to connect to k8s.example.com:6443: Connection refused"), {}, response), PB::Common::ResultCode::UNKNOWN)
      << join_lines(response);
  EXPECT_EQ(
      join_lines(response),
      "Failed to connect to Kubernetes API server at 'https://k8s.example.com:6443' (settings): Failed to connect to k8s.example.com:6443: Connection refused");
}

TEST(CheckKubernetes, MissingConfigurationIsUnknownBeforeAnyRequest) {
  fake_api api;
  PB::Commands::QueryResponseMessage::Response response;
  kube_checks::settings none;
  EXPECT_EQ(run_cluster(api.factory(), {}, response, none), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
  EXPECT_NE(join_lines(response).find("No Kubernetes API server configured"), std::string::npos) << join_lines(response);
  EXPECT_TRUE(api.requests.empty());
}

TEST(CheckKubernetes, MetricsServerAbsenceIsNamed) {
  kube_checks::cluster target;
  target.host = "k8s.example.com";
  target.port = "6443";
  const kube_checks::kube_http_error e(404, "HTTP 404 Not Found", "");
  EXPECT_EQ(kube_checks::describe_http_error(target, "/apis/metrics.k8s.io/v1beta1/pods", e),
            "metrics-server is not installed in the cluster at 'https://k8s.example.com:6443' (HTTP 404 for GET /apis/metrics.k8s.io/v1beta1/pods)");
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
  s.token.clear();
  EXPECT_FALSE(kube_checks::resolve_cluster(s, c, error));
  EXPECT_NE(error.find("set `token` or `token file`"), std::string::npos) << error;
}

const char *KUBECONFIG = R"json({
  "apiVersion": "v1", "kind": "Config",
  "current-context": "prod",
  "clusters": [
    {"name": "prod-cluster", "cluster": {"server": "https://prod.example.com:6443", "certificate-authority-data": "LS0tLS1CRUdJTiBDRVJUSUZJQ0FURS0tLS0tCg=="}},
    {"name": "lab-cluster", "cluster": {"server": "https://lab.example.com", "insecure-skip-tls-verify": true}}
  ],
  "contexts": [
    {"name": "prod", "context": {"cluster": "prod-cluster", "user": "prod-user"}},
    {"name": "lab", "context": {"cluster": "lab-cluster", "user": "lab-user"}},
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
  EXPECT_EQ(c.verify_mode, "none") << "insecure-skip-tls-verify maps to verify mode none";
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

TEST(KubeClient, SecondsSinceHandlesTheApiTimestampForms) {
  EXPECT_EQ(kube_checks::seconds_since(""), -1);
  EXPECT_EQ(kube_checks::seconds_since("0001-01-01T00:00:00Z"), -1);
  EXPECT_EQ(kube_checks::seconds_since("not a date"), -1);
  EXPECT_GT(kube_checks::seconds_since("2020-01-01T00:00:00Z"), 24 * 3600LL * 365 * 5);
  EXPECT_GT(kube_checks::seconds_since("2020-01-01T00:00:00.123456Z"), 24 * 3600LL * 365 * 5);
}

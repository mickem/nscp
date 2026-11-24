// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the CheckKubernetes module class: the settings it registers
// and that every dispatch method resolves the cluster from those settings -
// shown with configurations the resolver refuses before any connection is
// made. The check logic itself is covered by check_kubernetes_test through
// an injected fetcher; no test here reaches an API server.

#include "CheckKubernetes.h"

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/test_helpers.hpp>
#include <string>
#include <vector>

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

const char *const kRoot = "/settings/kubernetes";

typedef PB::Commands::QueryRequestMessage::Request request_type;
typedef PB::Commands::QueryResponseMessage::Response response_type;

std::string message_of(const response_type &r) { return r.lines_size() > 0 ? r.lines(0).message() : std::string(); }

class CheckKubernetesModule : public ::testing::Test {
 protected:
  nscapi::test_helpers::stub_core &core() { return nscapi::test_helpers::stub_core::instance(); }

  void SetUp() override {
    core().reset();
    module_.set_id(42);
  }
  void TearDown() override {
    module_.unloadModule();
    core().reset();
  }

  bool load() { return module_.loadModuleEx("", NSCAPI::dontStart); }

  typedef void (CheckKubernetes::*check_fn)(const request_type &, response_type *);
  // Every check, with the message it answered.
  std::vector<std::pair<std::string, std::string> > run_all() {
    const std::vector<std::pair<check_fn, std::string> > checks = {{&CheckKubernetes::check_kubernetes, "check_kubernetes"},
                                                                   {&CheckKubernetes::check_pods, "check_kubernetes_pods"},
                                                                   {&CheckKubernetes::check_nodes, "check_kubernetes_nodes"},
                                                                   {&CheckKubernetes::check_workloads, "check_kubernetes_workloads"}};
    std::vector<std::pair<std::string, std::string> > ret;
    for (const auto &c : checks) {
      request_type request;
      request.set_command(c.second);
      response_type response;
      (module_.*c.first)(request, &response);
      EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN) << c.second << ": " << message_of(response);
      ret.push_back(std::make_pair(c.second, message_of(response)));
    }
    return ret;
  }

  CheckKubernetes module_;
};

}  // namespace

// ============================================================================
// The settings contract
// ============================================================================

TEST_F(CheckKubernetesModule, LoadRegistersItsKeysWithTheDocumentedDefaults) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for(kRoot, "api server"), "") << "empty means detect the in-cluster server";
  EXPECT_EQ(core().default_for(kRoot, "token"), "");
  EXPECT_EQ(core().default_for(kRoot, "token file"), "");
  EXPECT_EQ(core().default_for(kRoot, "kubeconfig"), "");
  EXPECT_EQ(core().default_for(kRoot, "context"), "");
  EXPECT_EQ(core().default_for(kRoot, "timeout"), "30");
  EXPECT_EQ(core().default_for(kRoot, "max response size"), "64");
}

TEST_F(CheckKubernetesModule, TlsDefaultsVerifyTheServer) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for(kRoot, "ca"), "${ca-path}");
  EXPECT_EQ(core().default_for(kRoot, "verify mode"), "peer");
  EXPECT_EQ(core().default_for(kRoot, "tls version"), "tlsv1.2+");
}

TEST_F(CheckKubernetesModule, TheTokenIsRegisteredAsSensitive) {
  ASSERT_TRUE(load());

  EXPECT_TRUE(core().is_sensitive("token"));
  EXPECT_FALSE(core().is_sensitive("token file"));
}

// ============================================================================
// Every check resolves the cluster from the settings
// ============================================================================

// 0 would read as "no deadline" to the HTTP client: one stalled API server
// would hold the check and its thread forever.
TEST_F(CheckKubernetesModule, AZeroTimeoutIsRefusedByEveryCheck) {
  core().set_setting(kRoot, "timeout", "0");
  ASSERT_TRUE(load());

  for (const auto &r : run_all()) {
    EXPECT_EQ(r.second.find("Invalid `timeout` under [/settings/kubernetes]: 0"), 0u) << r.first << ": " << r.second;
  }
}

TEST_F(CheckKubernetesModule, ANegativeMaxResponseSizeIsRefused) {
  core().set_setting(kRoot, "max response size", "-1");
  ASSERT_TRUE(load());

  for (const auto &r : run_all()) {
    EXPECT_EQ(r.second.find("Invalid `max response size` under [/settings/kubernetes]: -1"), 0u) << r.first << ": " << r.second;
  }
}

TEST_F(CheckKubernetesModule, AnApiServerWithoutCredentialsIsRefused) {
  core().set_setting(kRoot, "api server", "https://k8s.example.invalid:6443");
  ASSERT_TRUE(load());

  for (const auto &r : run_all()) {
    EXPECT_EQ(r.second.find("No credentials for the Kubernetes API server at"), 0u) << r.first << ": " << r.second;
    EXPECT_NE(r.second.find("set `token` or `token file`"), std::string::npos) << r.second;
  }
}

TEST_F(CheckKubernetesModule, AMissingTokenFileIsRefused) {
  core().set_setting(kRoot, "api server", "https://k8s.example.invalid:6443");
  core().set_setting(kRoot, "token file", "/nonexistent-nscp-test/token");
  ASSERT_TRUE(load());

  for (const auto &r : run_all()) {
    EXPECT_EQ(r.second.find("Invalid `token file` under [/settings/kubernetes]"), 0u) << r.first << ": " << r.second;
  }
}

TEST_F(CheckKubernetesModule, AnUnreadableKubeconfigIsRefused) {
  core().set_setting(kRoot, "kubeconfig", "/nonexistent-nscp-test/kubeconfig.json");
  ASSERT_TRUE(load());

  for (const auto &r : run_all()) {
    EXPECT_EQ(r.second, "Failed to read kubeconfig '/nonexistent-nscp-test/kubeconfig.json' (`kubeconfig` under [/settings/kubernetes])") << r.first;
  }
}

// A reload re-reads the settings in place.
TEST_F(CheckKubernetesModule, ReloadPicksUpNewSettings) {
  core().set_setting(kRoot, "timeout", "0");
  ASSERT_TRUE(load());
  core().set_setting(kRoot, "timeout", "30");
  core().set_setting(kRoot, "max response size", "-5");
  ASSERT_TRUE(module_.loadModuleEx("", NSCAPI::reloadStart));

  for (const auto &r : run_all()) {
    EXPECT_EQ(r.second.find("Invalid `max response size`"), 0u) << r.first << ": " << r.second;
  }
}

// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the CheckDocker module class: the settings it registers,
// which endpoint every check and the facts round will (and will not) talk
// to, and the facts selection.
//
// The check logic is covered by check_docker_test through an injected
// fetcher. What is covered here is the module shell: that each dispatch
// method holds its request to the configured endpoint, and what the facts
// round does with each configuration. The only daemon any test reaches for
// is a unix socket path that does not exist (POSIX only), so nothing leaves
// the process and no docker installation is needed.

#include "CheckDocker.h"
#include "docker_facts.hpp"

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/protobuf/facts.hpp>
#include <nscapi/test_helpers.hpp>
#include <string>
#include <vector>

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

const char *const kRoot = "/settings/docker";
const char *const kMissingSocket = "/nonexistent-nscp-test/docker.sock";

typedef PB::Commands::QueryRequestMessage::Request request_type;
typedef PB::Commands::QueryResponseMessage::Response response_type;

std::string message_of(const response_type &r) { return r.lines_size() > 0 ? r.lines(0).message() : std::string(); }

nscapi::facts::request facts_request(const std::string &reason) {
  PB::Facts::FactsQueryMessage message;
  message.add_payload()->set_reason(reason);
  return nscapi::facts::request(message.SerializeAsString());
}

// The one docker set's error, or "" if the round produced none.
std::string docker_error(const nscapi::facts::response &response) {
  const PB::Facts::FactsMessage message = response.to_message();
  for (const PB::Facts::FactSet &set : message.payload(0).sets()) {
    if (set.id() == docker_facts::set_docker) return set.error();
  }
  return std::string();
}

class CheckDockerModule : public ::testing::Test {
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

  typedef void (CheckDocker::*check_fn)(const request_type &, response_type *);
  response_type run(check_fn fn, const std::string &command, const std::vector<std::string> &args = {}) {
    request_type request;
    request.set_command(command);
    for (const std::string &a : args) request.add_arguments(a);
    response_type response;
    (module_.*fn)(request, &response);
    return response;
  }

  std::vector<std::pair<check_fn, std::string> > all_checks() const {
    return {{&CheckDocker::check_docker, "check_docker"},
            {&CheckDocker::check_docker_info, "check_docker_info"},
            {&CheckDocker::check_docker_stats, "check_docker_stats"},
            {&CheckDocker::check_docker_restarts, "check_docker_restarts"},
            {&CheckDocker::check_docker_df, "check_docker_df"}};
  }

  CheckDocker module_;
};

}  // namespace

// ============================================================================
// The settings contract
// ============================================================================

TEST_F(CheckDockerModule, LoadRegistersItsKeysWithTheDocumentedDefaults) {
  ASSERT_TRUE(load());

#ifdef WIN32
  EXPECT_EQ(core().default_for(kRoot, "endpoint"), "\\\\.\\pipe\\docker_engine");
#else
  EXPECT_EQ(core().default_for(kRoot, "endpoint"), "/var/run/docker.sock");
#endif
  EXPECT_EQ(core().default_for(kRoot, "timeout"), "10");
}

TEST_F(CheckDockerModule, EveryFactsPartIsOffByDefault) {
  ASSERT_TRUE(load());

  const std::string facts = std::string(kRoot) + "/facts";
  EXPECT_EQ(core().default_for(facts, "docker"), "false");
  EXPECT_EQ(core().default_for(facts, "docker.containers"), "false");
  EXPECT_EQ(core().default_for(facts, "docker.images"), "false");
}

// ============================================================================
// Which daemon the checks talk to
// ============================================================================

// Which daemon the agent talks to is the operator's setting: a request may
// repeat it but not name another one.
TEST_F(CheckDockerModule, EveryCheckRefusesARequestSuppliedEndpoint) {
  ASSERT_TRUE(load());

  for (const auto &c : all_checks()) {
    const response_type r = run(c.first, c.second, {"host=/tmp/other.sock"});
    EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN) << c.second;
    EXPECT_EQ(message_of(r).find("Refusing a request-supplied docker endpoint"), 0u) << c.second << ": " << message_of(r);
  }
}

#ifndef WIN32
TEST_F(CheckDockerModule, ARelativeConfiguredEndpointIsRefused) {
  core().set_setting(kRoot, "endpoint", "docker.sock");
  ASSERT_TRUE(load());

  for (const auto &c : all_checks()) {
    EXPECT_EQ(message_of(run(c.first, c.second)), "Refusing docker endpoint 'docker.sock': expected an absolute path to the docker socket.") << c.second;
  }
}

TEST_F(CheckDockerModule, ATraversingConfiguredEndpointIsRefused) {
  core().set_setting(kRoot, "endpoint", "/var/run/../tmp/docker.sock");
  ASSERT_TRUE(load());

  EXPECT_EQ(message_of(run(&CheckDocker::check_docker, "check_docker")),
            "Refusing docker endpoint '/var/run/../tmp/docker.sock': path traversal is not allowed.");
}

// The configured endpoint is used: a socket that is not there is reported as
// a failure to reach the daemon, not as an empty container list.
TEST_F(CheckDockerModule, AnUnreachableDaemonIsUnknown) {
  core().set_setting(kRoot, "endpoint", kMissingSocket);
  core().set_setting(kRoot, "timeout", "1");
  ASSERT_TRUE(load());

  for (const auto &c : all_checks()) {
    const response_type r = run(c.first, c.second);
    EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN) << c.second << ": " << message_of(r);
    EXPECT_NE(message_of(r).find("Failed to connect to docker daemon at '" + std::string(kMissingSocket) + "'"), std::string::npos)
        << c.second << ": " << message_of(r);
  }
}
#endif

// ============================================================================
// Facts
// ============================================================================

TEST_F(CheckDockerModule, NoFactsAreProducedByDefault) {
  ASSERT_TRUE(load());

  nscapi::facts::response response;
  module_.fetchFacts(facts_request("scheduled"), response);
  EXPECT_EQ(response.to_message().payload(0).sets_size(), 0);
}

// A hung daemon must not hold the service start: the startup round claims
// the set and says when it will be read.
TEST_F(CheckDockerModule, FactsAreNotCollectedAtStartup) {
  core().set_setting(std::string(kRoot) + "/facts", "docker.containers", "true");
  ASSERT_TRUE(load());

  nscapi::facts::response response;
  module_.fetchFacts(facts_request("startup"), response);
  EXPECT_EQ(docker_error(response).find("Not collected during startup"), 0u) << docker_error(response);
}

#ifndef WIN32
TEST_F(CheckDockerModule, FactsHoldTheEndpointToTheSameRule) {
  core().set_setting(kRoot, "endpoint", "docker.sock");
  core().set_setting(std::string(kRoot) + "/facts", "docker", "true");
  ASSERT_TRUE(load());

  nscapi::facts::response response;
  module_.fetchFacts(facts_request("scheduled"), response);
  EXPECT_EQ(docker_error(response), "Refusing docker endpoint 'docker.sock': expected an absolute path to the docker socket.");
}

// A daemon that is down is named against the set, so the core keeps the
// inventory it already holds rather than blanking it.
TEST_F(CheckDockerModule, AnUnreachableDaemonIsAFactsErrorNotAnEmptyInventory) {
  core().set_setting(kRoot, "endpoint", kMissingSocket);
  core().set_setting(kRoot, "timeout", "1");
  core().set_setting(std::string(kRoot) + "/facts", "docker.images", "true");
  ASSERT_TRUE(load());

  nscapi::facts::response response;
  module_.fetchFacts(facts_request("manual"), response);
  EXPECT_NE(docker_error(response).find("Failed to connect to docker daemon at '" + std::string(kMissingSocket) + "'"), std::string::npos)
      << docker_error(response);
  EXPECT_EQ(response.to_message().payload(0).sets(0).facts().fields_size(), 0);
}
#endif

TEST_F(CheckDockerModule, ReloadTurnsFactsOff) {
  core().set_setting(std::string(kRoot) + "/facts", "docker.containers", "true");
  ASSERT_TRUE(load());
  core().set_setting(std::string(kRoot) + "/facts", "docker.containers", "false");
  ASSERT_TRUE(load());

  nscapi::facts::response response;
  module_.fetchFacts(facts_request("startup"), response);
  EXPECT_EQ(response.to_message().payload(0).sets_size(), 0);
}

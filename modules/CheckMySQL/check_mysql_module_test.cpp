// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the CheckMySQL module class: the settings it registers, the
// facts selection, and that both checks and the facts round connect with the
// configured defaults.
//
// The check logic is covered by check_mysql_test through an injected session
// factory. Here the real factory is used, pointed at a unix socket path that
// does not exist, so every connection fails locally and at once: no server,
// no network and no credentials are involved.

#include "CheckMySQL.h"
#include "mysql_facts.hpp"

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

const char *const kRoot = "/settings/mysql";
const char *const kMissingSocket = "/nonexistent-nscp-test/mysqld.sock";

typedef PB::Commands::QueryRequestMessage::Request request_type;
typedef PB::Commands::QueryResponseMessage::Response response_type;

std::string message_of(const response_type &r) { return r.lines_size() > 0 ? r.lines(0).message() : std::string(); }

nscapi::facts::request facts_request(const std::string &reason) {
  PB::Facts::FactsQueryMessage message;
  message.add_payload()->set_reason(reason);
  return nscapi::facts::request(message.SerializeAsString());
}

std::string mysql_error(const nscapi::facts::response &response) {
  const PB::Facts::FactsMessage message = response.to_message();
  for (const PB::Facts::FactSet &set : message.payload(0).sets()) {
    if (set.id() == mysql_facts::set_mysql) return set.error();
  }
  return std::string();
}

class CheckMySQLModule : public ::testing::Test {
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

  // Point the defaults at a socket that is not there.
  void unreachable() {
    core().set_setting(kRoot, "hostname", "localhost");
    core().set_setting(kRoot, "socket", kMissingSocket);
    core().set_setting(kRoot, "timeout", "1");
  }

  response_type check_mysql(const std::vector<std::string> &args = {}) {
    request_type request;
    request.set_command("check_mysql");
    for (const std::string &a : args) request.add_arguments(a);
    response_type response;
    module_.check_mysql(request, &response);
    return response;
  }
  response_type check_mysql_query(const std::vector<std::string> &args = {}) {
    request_type request;
    request.set_command("check_mysql_query");
    for (const std::string &a : args) request.add_arguments(a);
    response_type response;
    module_.check_mysql_query(request, &response);
    return response;
  }

  CheckMySQL module_;
};

}  // namespace

// ============================================================================
// The settings contract
// ============================================================================

TEST_F(CheckMySQLModule, LoadRegistersItsKeysWithTheDocumentedDefaults) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for(kRoot, "hostname"), "localhost");
  EXPECT_EQ(core().default_for(kRoot, "port"), "3306");
  EXPECT_EQ(core().default_for(kRoot, "socket"), "");
  EXPECT_EQ(core().default_for(kRoot, "user"), "");
  EXPECT_EQ(core().default_for(kRoot, "password"), "");
  EXPECT_EQ(core().default_for(kRoot, "database"), "");
  EXPECT_EQ(core().default_for(kRoot, "defaults file"), "");
  EXPECT_EQ(core().default_for(kRoot, "plugin dir"), "");
  EXPECT_EQ(core().default_for(kRoot, "tls"), "false");
  EXPECT_EQ(core().default_for(kRoot, "timeout"), "10");
  EXPECT_EQ(core().default_for(kRoot, "query timeout"), "30");
}

TEST_F(CheckMySQLModule, ThePasswordIsRegisteredAsSensitive) {
  ASSERT_TRUE(load());

  EXPECT_TRUE(core().is_sensitive("password"));
  EXPECT_FALSE(core().is_sensitive("user"));
}

TEST_F(CheckMySQLModule, FactsAreOffByDefault) {
  ASSERT_TRUE(load());

  const std::string facts = std::string(kRoot) + "/facts";
  EXPECT_EQ(core().default_for(facts, "mysql"), "false");
  EXPECT_EQ(core().default_for(facts, "mysql.databases"), "false");
}

// ============================================================================
// The checks
// ============================================================================

TEST_F(CheckMySQLModule, CheckMySqlUsesTheConfiguredConnection) {
  unreachable();
  ASSERT_TRUE(load());

  const response_type r = check_mysql();
  EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_EQ(message_of(r).find("Failed to connect to MySQL server '"), 0u) << message_of(r);
  EXPECT_NE(message_of(r).find(kMissingSocket), std::string::npos) << message_of(r);
}

TEST_F(CheckMySQLModule, CheckMySqlQueryNeedsAQuery) {
  unreachable();
  ASSERT_TRUE(load());

  EXPECT_EQ(message_of(check_mysql_query()), "No query specified (use query=<SQL>)");
}

TEST_F(CheckMySQLModule, CheckMySqlQueryUsesTheConfiguredConnection) {
  unreachable();
  ASSERT_TRUE(load());

  const response_type r = check_mysql_query({"query=SELECT 1"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_EQ(message_of(r).find("Failed to connect to MySQL server '"), 0u) << message_of(r);
}

TEST_F(CheckMySQLModule, AnUnknownOptionIsRejectedBeforeConnecting) {
  ASSERT_TRUE(load());

  const response_type r = check_mysql({"no-such-option=1"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(message_of(r).find("no-such-option"), std::string::npos) << message_of(r);
}

// ============================================================================
// Facts
// ============================================================================

TEST_F(CheckMySQLModule, NoFactsAreProducedByDefault) {
  ASSERT_TRUE(load());

  nscapi::facts::response response;
  module_.fetchFacts(facts_request("scheduled"), response);
  EXPECT_EQ(response.to_message().payload(0).sets_size(), 0);
}

// A server that is down must not hold the service start for the connect
// timeout: the startup round claims the set and says when it will be read.
TEST_F(CheckMySQLModule, FactsAreNotCollectedAtStartup) {
  core().set_setting(std::string(kRoot) + "/facts", "mysql", "true");
  ASSERT_TRUE(load());

  nscapi::facts::response response;
  module_.fetchFacts(facts_request("startup"), response);
  EXPECT_EQ(mysql_error(response).find("Not collected during startup"), 0u) << mysql_error(response);
}

// A failed round is named against the set, worded as the checks word it, so
// the core keeps the inventory it already holds.
TEST_F(CheckMySQLModule, AnUnreachableServerIsAFactsError) {
  unreachable();
  core().set_setting(std::string(kRoot) + "/facts", "mysql.databases", "true");
  ASSERT_TRUE(load());

  nscapi::facts::response response;
  module_.fetchFacts(facts_request("scheduled"), response);
  EXPECT_EQ(mysql_error(response).find("Failed to connect to MySQL server '"), 0u) << mysql_error(response);
}

TEST_F(CheckMySQLModule, ReloadTurnsFactsOff) {
  core().set_setting(std::string(kRoot) + "/facts", "mysql", "true");
  ASSERT_TRUE(load());
  core().set_setting(std::string(kRoot) + "/facts", "mysql", "false");
  ASSERT_TRUE(load());

  nscapi::facts::response response;
  module_.fetchFacts(facts_request("startup"), response);
  EXPECT_EQ(response.to_message().payload(0).sets_size(), 0);
}

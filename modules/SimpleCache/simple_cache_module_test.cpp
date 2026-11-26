// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the SimpleCache module class: the settings it registers, the
// channel it listens on, how `primary index` turns a submission (and a
// check_cache query) into a cache key, and the check_cache / list_cache
// commands themselves. The module has no thread and no socket, so the whole
// round trip - submit, then query - runs in process through the stub core.

#include "SimpleCache.h"

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/test_helpers.hpp>
#include <string>
#include <vector>

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

class SimpleCacheModule : public ::testing::Test {
 protected:
  nscapi::test_helpers::stub_core &core() { return nscapi::test_helpers::stub_core::instance(); }

  void SetUp() override {
    core().reset();
    module_.set_id(42);
  }
  void TearDown() override { core().reset(); }

  bool load(const NSCAPI::moduleLoadMode mode = NSCAPI::dontStart) { return module_.loadModuleEx("", mode); }

  // Submit one result to the cache, as a channel delivery would.
  PB::Commands::SubmitResponseMessage::Response submit(const std::string &command, const std::string &message, const std::string &alias = "",
                                                       const std::string &host = "", const std::string &channel = "CACHE") {
    PB::Commands::SubmitRequestMessage request_message;
    if (!host.empty()) {
      request_message.mutable_header()->set_sender_id("sender");
      PB::Common::Host *h = request_message.mutable_header()->add_hosts();
      h->set_id("other");
      h->set_host("not-this-one");
      h = request_message.mutable_header()->add_hosts();
      h->set_id("sender");
      h->set_host(host);
    }
    PB::Commands::QueryResponseMessage::Response payload;
    payload.set_command(command);
    if (!alias.empty()) payload.set_alias(alias);
    payload.set_result(PB::Common::ResultCode::WARNING);
    payload.add_lines()->set_message(message);
    PB::Commands::SubmitResponseMessage::Response response;
    module_.handleNotification(channel, payload, &response, request_message);
    return response;
  }

  PB::Commands::QueryResponseMessage::Response check_cache(const std::vector<std::string> &args) {
    PB::Commands::QueryRequestMessage::Request request;
    request.set_command("check_cache");
    for (const std::string &a : args) request.add_arguments(a);
    PB::Commands::QueryResponseMessage::Response response;
    module_.check_cache(request, &response);
    return response;
  }

  std::string list_cache() {
    PB::Commands::QueryRequestMessage::Request request;
    request.set_command("list_cache");
    PB::Commands::QueryResponseMessage::Response response;
    module_.list_cache(request, &response);
    return response.lines_size() > 0 ? response.lines(0).message() : std::string();
  }

  static std::string message_of(const PB::Commands::QueryResponseMessage::Response &r) { return r.lines_size() > 0 ? r.lines(0).message() : std::string(); }

  SimpleCache module_;
};

}  // namespace

// ============================================================================
// The settings contract
// ============================================================================

TEST_F(SimpleCacheModule, LoadRegistersItsKeysWithTheDocumentedDefaults) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for("/settings/cache", "primary index"), "${alias-or-command}");
  EXPECT_EQ(core().default_for("/settings/cache", "channel"), "CACHE");
}

TEST_F(SimpleCacheModule, LoadRegistersTheDefaultChannel) {
  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_channel("CACHE"));
}

TEST_F(SimpleCacheModule, ConfiguredChannelIsRegisteredInsteadOfTheDefault) {
  core().set_setting("channel", "MY_CACHE");

  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_channel("MY_CACHE"));
  EXPECT_FALSE(core().has_channel("CACHE"));
}

TEST_F(SimpleCacheModule, AnUnknownIndexVariableIsReported) {
  core().set_setting("primary index", "${command}-${bogus}");

  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_logged("Invalid index: bogus"));
}

// ============================================================================
// Submitting and fetching
// ============================================================================

TEST_F(SimpleCacheModule, ASubmissionIsAcknowledgedAsCached) {
  ASSERT_TRUE(load());

  const PB::Commands::SubmitResponseMessage::Response r = submit("check_cpu", "CPU load ok");
  EXPECT_EQ(r.command(), "check_cpu");
  EXPECT_EQ(r.result().code(), PB::Common::Result_StatusCodeType_STATUS_OK);
  EXPECT_EQ(r.result().message(), "message has been cached");
}

// The cached payload comes back as it was submitted, status included.
TEST_F(SimpleCacheModule, ACachedResultIsReturnedVerbatim) {
  ASSERT_TRUE(load());
  submit("check_cpu", "CPU load ok");

  const PB::Commands::QueryResponseMessage::Response r = check_cache({"command=check_cpu"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::WARNING);
  EXPECT_EQ(message_of(r), "CPU load ok");
  EXPECT_EQ(r.command(), "check_cpu");
}

// The default index is the alias when there is one, the command otherwise -
// on both the submitting and the querying side.
TEST_F(SimpleCacheModule, TheDefaultIndexPrefersTheAlias) {
  ASSERT_TRUE(load());
  submit("check_cpu", "aliased", "cpu_alias");

  EXPECT_EQ(list_cache(), "cpu_alias");
  EXPECT_EQ(message_of(check_cache({"alias=cpu_alias"})), "aliased");
  EXPECT_EQ(message_of(check_cache({"alias=cpu_alias", "command=ignored"})), "aliased");
  EXPECT_EQ(check_cache({"command=check_cpu"}).result(), PB::Common::ResultCode::UNKNOWN) << "stored under the alias, not the command";
}

TEST_F(SimpleCacheModule, ANewerSubmissionReplacesTheOlder) {
  ASSERT_TRUE(load());
  submit("check_cpu", "first");
  submit("check_cpu", "second");

  EXPECT_EQ(message_of(check_cache({"command=check_cpu"})), "second");
  EXPECT_EQ(list_cache(), "check_cpu");
}

TEST_F(SimpleCacheModule, AMissingEntryIsUnknownByDefault) {
  ASSERT_TRUE(load());

  const PB::Commands::QueryResponseMessage::Response r = check_cache({"command=check_nothing"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_EQ(message_of(r), "Entry not found");
}

TEST_F(SimpleCacheModule, TheNotFoundMessageAndCodeAreConfigurable) {
  ASSERT_TRUE(load());

  const PB::Commands::QueryResponseMessage::Response r = check_cache({"command=check_nothing", "not-found-msg=gone", "not-found-code=critical"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::CRITICAL);
  EXPECT_EQ(message_of(r), "gone");
}

// An explicit key is used as it is, bypassing the index expression.
TEST_F(SimpleCacheModule, AnExplicitKeyIsLookedUpVerbatim) {
  core().set_setting("primary index", "${host}/${command}");
  ASSERT_TRUE(load());
  submit("check_cpu", "from web01", "", "web01");

  EXPECT_EQ(message_of(check_cache({"key=web01/check_cpu"})), "from web01");
}

TEST_F(SimpleCacheModule, AQueryThatBuildsNoKeyIsASyntaxError) {
  ASSERT_TRUE(load());

  const PB::Commands::QueryResponseMessage::Response r = check_cache({});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(message_of(r).find("No key specified"), std::string::npos) << message_of(r);
}

TEST_F(SimpleCacheModule, AnUnknownOptionIsASyntaxError) {
  ASSERT_TRUE(load());

  const PB::Commands::QueryResponseMessage::Response r = check_cache({"no-such-option=1"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(message_of(r).find("Invalid command line"), std::string::npos) << message_of(r);
}

// ============================================================================
// The index expression
// ============================================================================

// The host is the one the header names as the sender, not just any host.
TEST_F(SimpleCacheModule, HostIndexUsesTheSendingHost) {
  core().set_setting("primary index", "${host}/${command}");
  ASSERT_TRUE(load());
  submit("check_cpu", "from web01", "", "web01");

  EXPECT_EQ(list_cache(), "web01/check_cpu");
  EXPECT_EQ(message_of(check_cache({"host=web01", "command=check_cpu"})), "from web01");
  EXPECT_EQ(check_cache({"host=web02", "command=check_cpu"}).result(), PB::Common::ResultCode::UNKNOWN);
}

TEST_F(SimpleCacheModule, ChannelAndAliasIndexes) {
  core().set_setting("primary index", "${channel}:${alias}");
  ASSERT_TRUE(load());
  submit("check_cpu", "via channel", "cpu", "", "INBOX");

  EXPECT_EQ(list_cache(), "INBOX:cpu");
  EXPECT_EQ(message_of(check_cache({"channel=INBOX", "alias=cpu"})), "via channel");
}

TEST_F(SimpleCacheModule, ListCacheListsEveryKey) {
  core().set_setting("primary index", "${command}");
  ASSERT_TRUE(load());
  EXPECT_EQ(list_cache(), "");

  submit("check_cpu", "a");
  submit("check_memory", "b");
  EXPECT_EQ(list_cache(), "check_cpu, check_memory");
}

// A reload rebuilds the index from scratch: appending to the old list would
// turn every key into the expression repeated once per reload.
TEST_F(SimpleCacheModule, ReloadDoesNotDuplicateTheIndex) {
  core().set_setting("primary index", "${command}");
  ASSERT_TRUE(load());
  ASSERT_TRUE(load(NSCAPI::reloadStart));
  submit("check_cpu", "a");

  EXPECT_EQ(list_cache(), "check_cpu");
}

TEST_F(SimpleCacheModule, ReloadPicksUpANewIndexAndKeepsTheCache) {
  core().set_setting("primary index", "${command}");
  ASSERT_TRUE(load());
  submit("check_cpu", "old key");

  core().set_setting("primary index", "x-${command}");
  ASSERT_TRUE(load(NSCAPI::reloadStart));
  submit("check_cpu", "new key");

  EXPECT_EQ(list_cache(), "check_cpu, x-check_cpu");
  EXPECT_EQ(message_of(check_cache({"command=check_cpu"})), "new key");
  EXPECT_EQ(message_of(check_cache({"key=check_cpu"})), "old key");
}

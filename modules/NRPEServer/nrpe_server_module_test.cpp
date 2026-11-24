// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the NRPEServer module class: the settings it registers, the
// configurations a start refuses, and handle() - what the server does with a
// decoded NRPE packet: the ping, the argument and metacharacter policy, the
// query through the core and how its answer becomes one or more response
// packets.
//
// No test opens a socket. Settings loads use dontStart; the refusing loads
// use normalStart and every one of them returns before the listener is
// built. handle() is driven directly, the way the connection layer calls it,
// with the stub core answering the query.

#include "NRPEServer.h"

#include <gtest/gtest.h>

#include <list>
#include <net/nrpe/packet.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/test_helpers.hpp>
#include <string>

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

const char *const kRoot = "/settings/NRPE/server";

class NrpeServerModule : public ::testing::Test {
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

  bool load(const NSCAPI::moduleLoadMode mode = NSCAPI::dontStart) { return module_.loadModuleEx("", mode); }

  std::list<nrpe::packet> handle(const std::string &payload, const unsigned int length = 1024) {
    return module_.handle(nrpe::packet::make_request(payload, length, nrpe::data::version2), "");
  }

  // A query answer carrying perfdata, for the message|perf rendering.
  static PB::Commands::QueryResponseMessage answer_with_perf(const std::string &command, const std::string &message) {
    PB::Commands::QueryResponseMessage reply;
    PB::Commands::QueryResponseMessage::Response *p = reply.add_payload();
    p->set_command(command);
    p->set_result(PB::Common::ResultCode::WARNING);
    PB::Commands::QueryResponseMessage::Response::Line *line = p->add_lines();
    line->set_message(message);
    PB::Common::PerformanceData *perf = line->add_perf();
    perf->set_alias("load");
    perf->mutable_float_value()->set_value(5);
    return reply;
  }

  nscapi::test_helpers::stdout_capture out_;
  NRPEServer module_;
};

}  // namespace

// ============================================================================
// The settings contract
// ============================================================================

TEST_F(NrpeServerModule, LoadRegistersItsKeysWithTheDocumentedDefaults) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for(kRoot, "port"), "5666");
  EXPECT_EQ(core().default_for(kRoot, "payload length"), "1024");
  EXPECT_EQ(core().default_for(kRoot, "allow arguments"), "false");
  EXPECT_EQ(core().default_for(kRoot, "allow nasty characters"), "false");
  EXPECT_EQ(core().default_for(kRoot, "performance data"), "true");
  EXPECT_EQ(core().default_for(kRoot, "insecure"), "false");
  EXPECT_EQ(core().default_for(kRoot, "expose version"), "true");
  EXPECT_EQ(core().default_for(kRoot, "client identity source"), "none");
}

TEST_F(NrpeServerModule, TheSecurePresetIsTheDefault) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for(kRoot, "certificate"), "${certificate-path}/certificate.pem");
  EXPECT_EQ(core().default_for(kRoot, "allowed ciphers"), "ALL:!aNULL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
  EXPECT_EQ(core().default_for(kRoot, "dh"), "${nrpe-dh}/nrpe_dh_2048.pem");
  EXPECT_EQ(core().default_for(kRoot, "extended response"), "true");
}

// insecure=true is the legacy check_nrpe interop preset: anonymous ciphers, no
// certificate, and no extended response (an unmodified client cannot read it).
TEST_F(NrpeServerModule, TheInsecurePresetChangesTheDefaults) {
  core().set_setting(kRoot, "insecure", "true");

  ASSERT_TRUE(load());
  EXPECT_EQ(core().default_for(kRoot, "certificate"), "");
  EXPECT_EQ(core().default_for(kRoot, "allowed ciphers"), "ALL:!MD5:@STRENGTH:@SECLEVEL=0");
  EXPECT_EQ(core().default_for(kRoot, "extended response"), "false");
}

TEST_F(NrpeServerModule, SharedServerKeysAreRegisteredUnderTheDefaultSection) {
  ASSERT_TRUE(load());

  EXPECT_TRUE(core().has_key("/settings/default", "allowed hosts"));
  EXPECT_TRUE(core().has_key("/settings/default", "encoding"));
}

// ============================================================================
// Configurations a start refuses (before any socket is opened)
// ============================================================================

// A payload length of 0 would underflow the split arithmetic in handle().
TEST_F(NrpeServerModule, ATinyPayloadLengthIsRefused) {
  core().set_setting(kRoot, "payload length", "8");

  EXPECT_TRUE(load()) << "dontStart only reads the settings";
  EXPECT_FALSE(load(NSCAPI::normalStart));
  EXPECT_TRUE(core().has_logged("NRPE payload length 8 is too small (minimum 16)"));
}

TEST_F(NrpeServerModule, AnUnknownIdentitySourceIsRefused) {
  core().set_setting(kRoot, "client identity source", "dn");

  EXPECT_FALSE(load(NSCAPI::normalStart));
  EXPECT_TRUE(core().has_logged("unknown 'client identity source' value 'dn'"));
}

// The CN of an unverified certificate is whatever the client chose: cn mode
// needs SSL that demands and verifies a peer certificate against a CA.
TEST_F(NrpeServerModule, CnIdentityWithoutPeerVerificationIsRefused) {
  core().set_setting(kRoot, "client identity source", "cn");

  EXPECT_FALSE(load(NSCAPI::normalStart));
  EXPECT_TRUE(core().has_logged("'client identity source = cn' requires SSL with verify_mode containing 'peer'"));
  EXPECT_TRUE(core().has_logged("Refusing to start with attacker-supplied identity."));
}

TEST_F(NrpeServerModule, CnIdentityWithAnInvalidVerifyModeIsRefused) {
  core().set_setting(kRoot, "client identity source", "cn");
  core().set_setting(kRoot, "verify mode", "sometimes");

  EXPECT_FALSE(load(NSCAPI::reloadStart));
  EXPECT_TRUE(core().has_logged("'verify mode' is not valid"));
}

TEST_F(NrpeServerModule, UnloadAndShutdownWithoutAListenerAreSafe) {
  ASSERT_TRUE(load());
  module_.prepareShutdown();
  EXPECT_TRUE(module_.unloadModule());
  EXPECT_TRUE(module_.unloadModule());
}

// ============================================================================
// handle(): the ping
// ============================================================================

TEST_F(NrpeServerModule, ThePingRepliesWithTheVersion) {
  core().set_application_version("1.2.3");
  ASSERT_TRUE(load());

  const std::list<nrpe::packet> packets = handle("_NRPE_CHECK");
  ASSERT_EQ(packets.size(), 1u);
  EXPECT_EQ(packets.front().getResult(), NSCAPI::query_return_codes::returnOK);
  EXPECT_EQ(packets.front().getPayload(), "I (1.2.3) seem to be doing fine...");
  EXPECT_TRUE(core().queries().empty()) << "the ping is answered without a query";
}

TEST_F(NrpeServerModule, ThePingCanHideTheVersion) {
  core().set_application_version("1.2.3");
  core().set_setting(kRoot, "expose version", "false");
  ASSERT_TRUE(load());

  const std::list<nrpe::packet> packets = handle("_NRPE_CHECK");
  ASSERT_EQ(packets.size(), 1u);
  EXPECT_EQ(packets.front().getPayload(), "I seem to be doing fine...");
}

// ============================================================================
// handle(): the request policy
// ============================================================================

TEST_F(NrpeServerModule, ArgumentsAreRefusedByDefault) {
  ASSERT_TRUE(load());

  EXPECT_THROW(handle("check_cpu!warn=load>80"), nrpe::nrpe_exception);
  EXPECT_TRUE(core().queries().empty());
}

TEST_F(NrpeServerModule, NastyCharactersInTheCommandAreRefused) {
  ASSERT_TRUE(load());

  EXPECT_THROW(handle("check_cpu|rm"), nrpe::nrpe_exception);
  EXPECT_TRUE(core().queries().empty());
}

TEST_F(NrpeServerModule, NastyCharactersInTheArgumentsAreRefused) {
  core().set_setting(kRoot, "allow arguments", "true");
  ASSERT_TRUE(load());

  EXPECT_THROW(handle("check_cpu!a`b`"), nrpe::nrpe_exception);
  EXPECT_TRUE(core().queries().empty());
}

TEST_F(NrpeServerModule, AllowedArgumentsAreSplitOnBang) {
  core().set_setting(kRoot, "allow arguments", "true");
  core().set_query_result("check_cpu", PB::Common::ResultCode::OK, "fine");
  ASSERT_TRUE(load());

  handle("check_cpu!warn=load gt 80!crit=load gt 90");
  ASSERT_EQ(core().queries().size(), 1u);
  const PB::Commands::QueryRequestMessage::Request &q = core().queries()[0].payload(0);
  EXPECT_EQ(q.command(), "check_cpu");
  ASSERT_EQ(q.arguments_size(), 2);
  EXPECT_EQ(q.arguments(0), "warn=load gt 80");
  EXPECT_EQ(q.arguments(1), "crit=load gt 90");
}

// ============================================================================
// handle(): the answer
// ============================================================================

TEST_F(NrpeServerModule, TheQueryResultIsReturned) {
  core().set_query_result("check_cpu", PB::Common::ResultCode::CRITICAL, "CPU on fire");
  ASSERT_TRUE(load());

  const std::list<nrpe::packet> packets = handle("check_cpu");
  ASSERT_EQ(packets.size(), 1u);
  EXPECT_EQ(packets.front().getResult(), NSCAPI::query_return_codes::returnCRIT);
  EXPECT_EQ(packets.front().getPayload(), "CPU on fire");
}

TEST_F(NrpeServerModule, PerfdataIsAppendedAfterAPipe) {
  core().set_query_response("check_cpu", answer_with_perf("check_cpu", "CPU high"));
  ASSERT_TRUE(load());

  const std::list<nrpe::packet> packets = handle("check_cpu");
  ASSERT_EQ(packets.size(), 1u);
  EXPECT_EQ(packets.front().getResult(), NSCAPI::query_return_codes::returnWARN);
  EXPECT_EQ(packets.front().getPayload(), "CPU high|'load'=5");
}

TEST_F(NrpeServerModule, PerfdataCanBeTurnedOff) {
  core().set_setting(kRoot, "performance data", "false");
  core().set_query_response("check_cpu", answer_with_perf("check_cpu", "CPU high"));
  ASSERT_TRUE(load());

  EXPECT_EQ(handle("check_cpu").front().getPayload(), "CPU high");
}

// Pinned with extended response off: with it on, an answer with no text
// (which is what the stub's "no such command" is) produces no packet at all
// - the split loop runs zero times.
TEST_F(NrpeServerModule, AnUnknownCommandIsUnknown) {
  core().set_setting(kRoot, "extended response", "false");
  ASSERT_TRUE(load());

  const std::list<nrpe::packet> packets = handle("check_nothing");
  ASSERT_EQ(packets.size(), 1u);
  EXPECT_EQ(packets.front().getResult(), NSCAPI::query_return_codes::returnUNKNOWN);
}

// With extended response a message longer than the payload is split over
// several packets, every one but the last a "more" packet.
TEST_F(NrpeServerModule, ALongAnswerIsSplitWithExtendedResponse) {
  core().set_query_result("check_big", PB::Common::ResultCode::OK, std::string(40, 'x'));
  ASSERT_TRUE(load());

  const std::list<nrpe::packet> packets = handle("check_big", 16);
  ASSERT_EQ(packets.size(), 3u) << "40 bytes over 15 byte packets";
  std::string joined;
  for (const nrpe::packet &p : packets) joined += p.getPayload();
  EXPECT_EQ(joined, std::string(40, 'x'));
  EXPECT_EQ(packets.front().getType(), nrpe::data::moreResponsePacket);
  EXPECT_EQ(packets.back().getType(), nrpe::data::responsePacket);
}

TEST_F(NrpeServerModule, ALongAnswerIsTruncatedWithoutExtendedResponse) {
  core().set_setting(kRoot, "extended response", "false");
  core().set_query_result("check_big", PB::Common::ResultCode::OK, std::string(40, 'x'));
  ASSERT_TRUE(load());

  const std::list<nrpe::packet> packets = handle("check_big", 16);
  ASSERT_EQ(packets.size(), 1u);
  EXPECT_EQ(packets.front().getPayload(), std::string(15, 'x'));
}

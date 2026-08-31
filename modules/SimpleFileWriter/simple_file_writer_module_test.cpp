// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the SimpleFileWriter module class: the settings it registers
// and reads in loadModuleEx(), the syntax it compiles into a list of lookup
// functors, and the line it writes for a submitted result.
//
// The whole module is one shell - there is no separate implementation file to
// test - so this covers the ${...} keyword table, the host/service syntax
// fallback and the file write end to end, all through the public interface.

#include "SimpleFileWriter.h"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <fstream>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/test_helpers.hpp>
#include <sstream>
#include <string>

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace fs = boost::filesystem;

namespace {

class SimpleFileWriterModule : public ::testing::Test {
 protected:
  nscapi::test_helpers::stub_core &core() { return nscapi::test_helpers::stub_core::instance(); }

  void SetUp() override {
    core().reset();
    module_.set_id(42);
    scratch_ = fs::temp_directory_path() / fs::unique_path("nscp-filewriter-%%%%%%%%");
    fs::create_directories(scratch_);
    output_ = (scratch_ / "output.txt").string();
    core().set_setting("file", output_);
  }
  void TearDown() override {
    core().reset();
    boost::system::error_code ec;
    fs::remove_all(scratch_, ec);
  }

  bool load() { return module_.loadModuleEx("", NSCAPI::dontStart); }

  // Submit one result through the module and return the line it wrote.
  std::string submit(const std::string &command, const std::string &message, const PB::Common::ResultCode result = PB::Common::ResultCode::OK,
                     const std::string &alias = "", const std::string &host = "") {
    PB::Commands::SubmitRequestMessage request_message;
    if (!host.empty()) request_message.mutable_header()->set_recipient_id(host);
    PB::Commands::QueryResponseMessage::Response payload;
    payload.set_command(command);
    if (!alias.empty()) payload.set_alias(alias);
    payload.add_lines()->set_message(message);
    payload.set_result(result);

    PB::Commands::SubmitResponseMessage::Response response;
    module_.handleNotification("FILE", payload, &response, request_message);
    return read_output();
  }

  std::string read_output() const {
    std::ifstream in(output_.c_str());
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
  }

  fs::path scratch_;
  std::string output_;
  SimpleFileWriter module_;
};

}  // namespace

// ============================================================================
// The settings contract
// ============================================================================

TEST_F(SimpleFileWriterModule, LoadRegistersItsKeysWithTheDocumentedDefaults) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for("syntax"), "${alias-or-command} ${result} ${message}");
  EXPECT_EQ(core().default_for("file"), "output.txt");
  EXPECT_EQ(core().default_for("channel"), "FILE");
  EXPECT_EQ(core().default_for("time-syntax"), "%Y-%m-%d %H:%M:%S");
  // Both fall back to `syntax` when unset, so they have no default of their own.
  EXPECT_EQ(core().default_for("host-syntax"), "");
  EXPECT_EQ(core().default_for("service-syntax"), "");
}

TEST_F(SimpleFileWriterModule, LoadRegistersTheDefaultChannel) {
  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_channel("FILE"));
}

TEST_F(SimpleFileWriterModule, ConfiguredChannelIsRegisteredInsteadOfTheDefault) {
  core().set_setting("channel", "MY_FILE");

  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_channel("MY_FILE"));
  EXPECT_FALSE(core().has_channel("FILE"));
}

// ============================================================================
// Writing: the default syntax, end to end
// ============================================================================

TEST_F(SimpleFileWriterModule, WritesTheDefaultSyntaxLine) {
  ASSERT_TRUE(load());

  const std::string written = submit("check_cpu", "OK: cpu load is fine");

  EXPECT_EQ(written, "check_cpu OK OK: cpu load is fine\n");
}

TEST_F(SimpleFileWriterModule, AppendsRatherThanTruncating) {
  ASSERT_TRUE(load());

  submit("check_cpu", "first");
  const std::string written = submit("check_mem", "second");

  EXPECT_EQ(written, "check_cpu OK first\ncheck_mem OK second\n");
}

// ============================================================================
// The ${...} keyword table
// ============================================================================

TEST_F(SimpleFileWriterModule, CommandAndMessageKeywords) {
  core().set_setting("syntax", "${command}|${message}");
  ASSERT_TRUE(load());

  EXPECT_EQ(submit("check_cpu", "hello"), "check_cpu|hello\n");
}

TEST_F(SimpleFileWriterModule, AliasKeywordsPreferTheAlias) {
  core().set_setting("syntax", "${alias}|${alias-or-command}");
  ASSERT_TRUE(load());

  EXPECT_EQ(submit("check_cpu", "hello", PB::Common::ResultCode::OK, "cpu_alias"), "cpu_alias|cpu_alias\n");
}

// alias-or-command is the default syntax's lead-in, so its fallback matters.
TEST_F(SimpleFileWriterModule, AliasOrCommandFallsBackToTheCommand) {
  core().set_setting("syntax", "${alias}|${alias-or-command}");
  ASSERT_TRUE(load());

  EXPECT_EQ(submit("check_cpu", "hello"), "|check_cpu\n");
}

TEST_F(SimpleFileWriterModule, ResultKeywordsRenderWordAndNumber) {
  core().set_setting("syntax", "${result}/${result_number}");
  ASSERT_TRUE(load());

  EXPECT_EQ(submit("check_cpu", "bad", PB::Common::ResultCode::CRITICAL), "CRITICAL/2\n");
}

// ${channel} is documented as "the receiving channel", but handleNotification
// drops its channel argument and passes request.command() into the functor's
// channel slot, so it renders the command instead. Pinned as it behaves today:
// changing it would change what every existing writer configuration produces.
TEST_F(SimpleFileWriterModule, ChannelKeywordCurrentlyRendersTheCommand) {
  core().set_setting("syntax", "[${channel}]");
  ASSERT_TRUE(load());

  EXPECT_EQ(submit("check_cpu", "hello"), "[check_cpu]\n");
}

TEST_F(SimpleFileWriterModule, EpochKeywordIsANumber) {
  core().set_setting("syntax", "${epoch}");
  ASSERT_TRUE(load());

  const std::string written = submit("check_cpu", "hello");
  ASSERT_FALSE(written.empty());
  EXPECT_NE(written.find_first_of("0123456789"), std::string::npos) << written;
}

TEST_F(SimpleFileWriterModule, TimeKeywordUsesTheConfiguredTimeSyntax) {
  core().set_setting("syntax", "${time}");
  core().set_setting("time-syntax", "%Y");
  ASSERT_TRUE(load());

  const std::string written = submit("check_cpu", "hello");
  EXPECT_EQ(written.size(), 5u) << written;  // four digits and the newline
}

// An unknown ${...} is reported and dropped; it must not abort the load or
// leave a half-built syntax behind.
TEST_F(SimpleFileWriterModule, UnknownKeywordIsSkipped) {
  core().set_setting("syntax", "a${not_a_keyword}b");
  ASSERT_TRUE(load());

  EXPECT_EQ(submit("check_cpu", "hello"), "ab\n");
}

// ============================================================================
// The host/service syntax split
// ============================================================================

// A payload with neither alias nor command is a host result and takes
// host-syntax; anything else is a service result and takes service-syntax.
TEST_F(SimpleFileWriterModule, ServiceAndHostSyntaxAreUsedForTheirOwnPayloads) {
  core().set_setting("syntax", "plain ${message}");
  core().set_setting("service-syntax", "service ${message}");
  core().set_setting("host-syntax", "host ${message}");
  ASSERT_TRUE(load());

  EXPECT_EQ(submit("check_cpu", "one"), "service one\n");
  EXPECT_EQ(submit("", "two"), "service one\nhost two\n");
}

// Both fall back to `syntax` when they are not configured.
TEST_F(SimpleFileWriterModule, UnsetHostAndServiceSyntaxFallBackToSyntax) {
  core().set_setting("syntax", "shared ${message}");
  ASSERT_TRUE(load());

  EXPECT_EQ(submit("check_cpu", "one"), "shared one\n");
  EXPECT_EQ(submit("", "two"), "shared one\nshared two\n");
}

// ============================================================================
// Failure handling
// ============================================================================

// The write target is a path the module cannot open. It must report the
// failure through the response rather than throwing out of the callback.
TEST_F(SimpleFileWriterModule, WriteToAnUnopenableFileIsHandled) {
  core().set_setting("file", (scratch_ / "no-such-dir" / "out.txt").string());
  ASSERT_TRUE(load());

  PB::Commands::SubmitRequestMessage request_message;
  PB::Commands::QueryResponseMessage::Response payload;
  payload.set_command("check_cpu");
  payload.add_lines()->set_message("hello");
  PB::Commands::SubmitResponseMessage::Response response;

  EXPECT_NO_THROW(module_.handleNotification("FILE", payload, &response, request_message));
}

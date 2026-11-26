// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the CheckNet module class: loadModuleEx, the argument
// validation check_ping does itself before it sends a packet, and that each
// dispatch method reaches its check - shown by a bad argument the check
// rejects before it touches the network.
//
// No test here sends anything: every request is refused during validation.
// The checks' own logic (filters, parsers, protocol encoders) is covered by
// check_net_test and its siblings.

#include "CheckNet.h"

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/test_helpers.hpp>
#include <string>
#include <vector>

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

typedef PB::Commands::QueryRequestMessage::Request request_type;
typedef PB::Commands::QueryResponseMessage::Response response_type;

request_type make_request(const std::string &command, const std::vector<std::string> &args) {
  request_type request;
  request.set_command(command);
  for (const std::string &a : args) request.add_arguments(a);
  return request;
}

std::string message_of(const response_type &r) { return r.lines_size() > 0 ? r.lines(0).message() : std::string(); }

class CheckNetModule : public ::testing::Test {
 protected:
  nscapi::test_helpers::stub_core &core() { return nscapi::test_helpers::stub_core::instance(); }

  void SetUp() override {
    core().reset();
    module_.set_id(42);
    ASSERT_TRUE(module_.loadModuleEx("", NSCAPI::dontStart));
  }
  void TearDown() override { core().reset(); }

  response_type ping(const std::vector<std::string> &args) {
    response_type response;
    CheckNet::check_ping(make_request("check_ping", args), &response);
    return response;
  }

  CheckNet module_;
};

}  // namespace

// The module has no settings of its own: loading registers nothing.
TEST_F(CheckNetModule, LoadRegistersNoSettings) {
  EXPECT_TRUE(core().registered_keys().empty());
  EXPECT_TRUE(core().registered_commands().empty()) << "commands are registered by the generated glue, not by loadModuleEx";
}

TEST_F(CheckNetModule, ReloadIsSafe) { EXPECT_TRUE(module_.loadModuleEx("", NSCAPI::reloadStart)); }

// ============================================================================
// check_ping: validation before anything is sent
// ============================================================================

TEST_F(CheckNetModule, PingWithoutAHostIsRefused) {
  const response_type r = ping({});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_EQ(message_of(r), "No host specified");
}

TEST_F(CheckNetModule, PingRejectsAnUnknownAddressFamily) {
  const response_type r = ping({"host=127.0.0.1", "address-family=ipx"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_EQ(message_of(r), "Invalid address-family: ipx (expected any, ipv4 or ipv6)");
}

TEST_F(CheckNetModule, PingRejectsANegativeSize) { EXPECT_EQ(message_of(ping({"host=127.0.0.1", "size=-1"})).find("Invalid size: -1 (expected 0-"), 0u); }

TEST_F(CheckNetModule, PingRejectsASizeAboveTheIcmpMaximum) {
  const std::string m = message_of(ping({"host=127.0.0.1", "size=70000"}));
  EXPECT_EQ(m.find("Invalid size: 70000 (expected 0-"), 0u) << m;
}

// Zero packets used to report 0% loss and OK without pinging anything.
TEST_F(CheckNetModule, PingRejectsAZeroCount) { EXPECT_EQ(message_of(ping({"host=127.0.0.1", "count=0"})), "Invalid count: 0 (expected 1 or more)"); }

TEST_F(CheckNetModule, PingRejectsATtlOutOfRange) {
  EXPECT_EQ(message_of(ping({"host=127.0.0.1", "ttl=256"})), "Invalid ttl: 256 (expected 0-255)");
  EXPECT_EQ(message_of(ping({"host=127.0.0.1", "ttl=-1"})), "Invalid ttl: -1 (expected 0-255)");
}

// count x payload is what one check can throw at a host, and anyone allowed
// to run checks picks both: the volume is capped at 10 MB per host.
TEST_F(CheckNetModule, PingRefusesMoreThanTenMegabytesPerHost) {
  const std::string m = message_of(ping({"host=127.0.0.1", "count=1000", "size=20000"}));
  EXPECT_EQ(m, "Refusing to send 20000000 bytes to each host (count=1000 x payload=20000 bytes); the limit is 10485760 bytes per host.");
}

TEST_F(CheckNetModule, PingWithAnEmptyHostListIsRefused) { EXPECT_EQ(message_of(ping({"hosts="})), "No host specified"); }

TEST_F(CheckNetModule, PingRejectsAnUnknownOption) {
  const response_type r = ping({"no-such-option=1"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_FALSE(message_of(r).empty());
}

// ============================================================================
// Dispatch: each method reaches its check
// ============================================================================

TEST_F(CheckNetModule, CheckTcpIsDispatched) {
  response_type r;
  module_.check_tcp(make_request("check_tcp", {}), &r);
  EXPECT_EQ(message_of(r), "No host specified");
}

TEST_F(CheckNetModule, CheckSshIsDispatched) {
  response_type r;
  module_.check_ssh(make_request("check_ssh", {"address-family=ipx"}), &r);
  EXPECT_EQ(message_of(r), "Invalid address-family: ipx (expected any, ipv4 or ipv6)");
}

TEST_F(CheckNetModule, CheckDnsIsDispatched) {
  response_type r;
  CheckNet::check_dns(make_request("check_dns", {}), &r);
  EXPECT_EQ(message_of(r), "No host specified");
}

TEST_F(CheckNetModule, CheckHttpIsDispatched) {
  response_type r;
  module_.check_http(make_request("check_http", {"timeout=0"}), &r);
  EXPECT_EQ(message_of(r), "timeout must be a positive number of milliseconds");
}

TEST_F(CheckNetModule, CheckNsclientWebOnlineIsDispatched) {
  response_type r;
  module_.check_nsclient_web_online(make_request("check_nsclient_web_online", {}), &r);
  EXPECT_EQ(message_of(r), "No host or url specified");
}

TEST_F(CheckNetModule, CheckNtpOffsetIsDispatched) {
  response_type r;
  CheckNet::check_ntp_offset(make_request("check_ntp_offset", {"address-family=ipx"}), &r);
  EXPECT_EQ(message_of(r), "Invalid address-family: ipx (expected any, ipv4 or ipv6)");
}

TEST_F(CheckNetModule, CheckRadiusIsDispatched) {
  response_type r;
  CheckNet::check_radius(make_request("check_radius", {}), &r);
  EXPECT_EQ(message_of(r), "host and secret-file are required");
}

TEST_F(CheckNetModule, CheckDomainIsDispatched) {
  response_type r;
  module_.check_domain(make_request("check_domain", {"timeout=0"}), &r);
  EXPECT_EQ(message_of(r), "timeout must be between 1 and 300 seconds");
}

// The web server status checks validate nothing of their own before the
// fetch, so an option they do not know is what shows the dispatch.
TEST_F(CheckNetModule, WebServerStatusChecksAreDispatched) {
  response_type apache, nginx, phpfpm, tomcat;
  module_.check_apache_status(make_request("check_apache_status", {"no-such-option=1"}), &apache);
  module_.check_nginx_status(make_request("check_nginx_status", {"no-such-option=1"}), &nginx);
  module_.check_phpfpm_status(make_request("check_phpfpm_status", {"no-such-option=1"}), &phpfpm);
  module_.check_tomcat_status(make_request("check_tomcat_status", {"no-such-option=1"}), &tomcat);
  for (const response_type *r : {&apache, &nginx, &phpfpm, &tomcat}) {
    EXPECT_EQ(r->result(), PB::Common::ResultCode::UNKNOWN);
    EXPECT_NE(message_of(*r).find("no-such-option"), std::string::npos) << message_of(*r);
  }
}

// check_jvm validates its attribute= aliases before the fetch, so a refused
// alias shows the dispatch without a request being sent.
TEST_F(CheckNetModule, CheckJvmIsDispatched) {
  response_type r;
  module_.check_jvm(make_request("check_jvm", {"attribute=in=java.lang:type=Threading/ThreadCount"}), &r);
  EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(message_of(r).find("Invalid attribute: alias in"), std::string::npos) << message_of(r);
}

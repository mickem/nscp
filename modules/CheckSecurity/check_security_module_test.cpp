// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the CheckSecurity module class: that it loads with no
// settings of its own and that every dispatch method reaches its check.
//
// The checks are covered by their own tests; what the module shell adds is
// the dispatch. It is shown with answers that do not depend on the host: on
// POSIX the Windows-only checks answer that they are not supported (the
// documented contract there), check_certificate is pointed at a file that
// does not exist, and the two checks that do read this host (check_users,
// check_firewall) are given an option they reject before reading anything.

#include "CheckSecurity.h"

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

std::string message_of(const response_type &r) { return r.lines_size() > 0 ? r.lines(0).message() : std::string(); }

class CheckSecurityModule : public ::testing::Test {
 protected:
  nscapi::test_helpers::stub_core &core() { return nscapi::test_helpers::stub_core::instance(); }

  void SetUp() override {
    core().reset();
    module_.set_id(42);
    ASSERT_TRUE(module_.loadModuleEx("", NSCAPI::dontStart));
  }
  void TearDown() override {
    EXPECT_TRUE(module_.unloadModule());
    core().reset();
  }

  typedef void (CheckSecurity::*check_fn)(const request_type &, response_type *);
  response_type run(check_fn fn, const std::string &command, const std::vector<std::string> &args = {}) {
    request_type request;
    request.set_command(command);
    for (const std::string &a : args) request.add_arguments(a);
    response_type response;
    (module_.*fn)(request, &response);
    return response;
  }

  CheckSecurity module_;
};

}  // namespace

TEST_F(CheckSecurityModule, LoadRegistersNoSettings) {
  EXPECT_TRUE(core().registered_keys().empty());
  EXPECT_TRUE(module_.loadModuleEx("", NSCAPI::reloadStart));
}

TEST_F(CheckSecurityModule, CheckCertificateReportsAMissingFile) {
  const response_type r = run(&CheckSecurity::check_certificate, "check_certificate", {"file=/nonexistent-nscp-test/cert.pem"});
  EXPECT_NE(r.result(), PB::Common::ResultCode::OK) << message_of(r);
  EXPECT_NE(message_of(r).find("/nonexistent-nscp-test/cert.pem"), std::string::npos) << message_of(r);
}

TEST_F(CheckSecurityModule, ChecksThatReadTheHostAreDispatched) {
  for (const auto &c :
       std::vector<std::pair<check_fn, std::string> >{{&CheckSecurity::check_users, "check_users"}, {&CheckSecurity::check_firewall, "check_firewall"}}) {
    const response_type r = run(c.first, c.second, {"no-such-option=1"});
    EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN) << c.second;
    EXPECT_NE(message_of(r).find("no-such-option"), std::string::npos) << c.second << ": " << message_of(r);
  }
}

#ifndef WIN32
// Off Windows these checks have nothing to read and say so, rather than
// reporting an empty (and therefore OK) result.
TEST_F(CheckSecurityModule, WindowsOnlyChecksSayTheyAreNotSupported) {
  const std::vector<std::pair<check_fn, std::string> > checks = {
      {&CheckSecurity::check_nla, "check_nla"},
      {&CheckSecurity::check_antivirus, "check_antivirus"},
      {&CheckSecurity::check_bitlocker, "check_bitlocker"},
      {&CheckSecurity::check_secureboot, "check_secureboot"},
      {&CheckSecurity::check_defender, "check_defender"},
      {&CheckSecurity::check_local_accounts, "check_local_accounts"},
      {&CheckSecurity::check_group_members, "check_group_members"},
      {&CheckSecurity::check_firewall_rules, "check_firewall_rules"},
      {&CheckSecurity::check_activation, "check_activation"},
  };
  for (const auto &c : checks) {
    const response_type r = run(c.first, c.second);
    EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN) << c.second;
    EXPECT_EQ(message_of(r).find(c.second + " is not supported on this platform"), 0u) << c.second << ": " << message_of(r);
  }
}

TEST_F(CheckSecurityModule, CheckFileSecurityIsNotSupported) {
  const response_type r = run(&CheckSecurity::check_file_security, "check_file_security", {"path=/tmp"});
  EXPECT_NE(r.result(), PB::Common::ResultCode::OK);
  EXPECT_NE(message_of(r).find("check_file_security is not supported on this platform"), std::string::npos) << message_of(r);
}
#endif

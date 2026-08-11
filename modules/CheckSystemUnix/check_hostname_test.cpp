// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_hostname.h"

#include <gtest/gtest.h>

using hostname_check::derive_identity;
using hostname_check::host_identity;

namespace {

std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

PB::Common::ResultCode run_check(const host_identity &info, const std::vector<std::string> &args, PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_hostname");
  for (const std::string &a : args) request.add_arguments(a);
  hostname_check::check_from(request, &response, info);
  return response.result();
}

}  // namespace

// --- derive_identity --------------------------------------------------------------

TEST(CheckHostname, DerivesDomainFromCanonicalName) {
  const host_identity h = derive_identity("web01", "web01.corp.example.com");
  EXPECT_EQ(h.fqdn, "web01.corp.example.com");
  EXPECT_EQ(h.domain, "corp.example.com");
  EXPECT_TRUE(h.fqdn_consistent);
}

TEST(CheckHostname, UnresolvableHostFallsBackToBareHostname) {
  const host_identity h = derive_identity("container123", "");
  EXPECT_EQ(h.fqdn, "container123");
  EXPECT_EQ(h.domain, "");
  EXPECT_TRUE(h.fqdn_consistent);  // no DNS is not drift
}

TEST(CheckHostname, CanonicalNameUnderDifferentLabelIsDrift) {
  // The resolver canonicalises this host as something else entirely (stale
  // /etc/hosts, CNAME chain, re-imaged box).
  const host_identity h = derive_identity("web01", "web01-old.corp.example.com");
  EXPECT_FALSE(h.fqdn_consistent);
  EXPECT_EQ(h.domain, "corp.example.com");
}

TEST(CheckHostname, CaseDifferencesAreNotDrift) {
  const host_identity h = derive_identity("web01", "WEB01.CORP.EXAMPLE.COM");
  EXPECT_TRUE(h.fqdn_consistent);
}

// --- rendering / thresholds ------------------------------------------------------

TEST(CheckHostname, DefaultIsOkIdentityLine) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(derive_identity("web01", "web01.corp.example.com"), {}, response), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_NE(join_lines(response).find("web01 (web01.corp.example.com), domain=corp.example.com"), std::string::npos) << join_lines(response);
}

TEST(CheckHostname, PinnedDomainMismatchIsCritical) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(derive_identity("web01", "web01.corp.example.com"), {"crit=domain != 'other.example.com'"}, response), PB::Common::ResultCode::CRITICAL)
      << join_lines(response);
  PB::Commands::QueryResponseMessage::Response ok_response;
  EXPECT_EQ(run_check(derive_identity("web01", "web01.corp.example.com"), {"crit=domain != 'corp.example.com'"}, ok_response), PB::Common::ResultCode::OK)
      << join_lines(ok_response);
}

TEST(CheckHostname, ConsistencyFlagIsThresholdable) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(derive_identity("web01", "web01-old.corp.example.com"), {"warn=fqdn_consistent = 0"}, response), PB::Common::ResultCode::WARNING)
      << join_lines(response);
  PB::Commands::QueryResponseMessage::Response ok_response;
  EXPECT_EQ(run_check(derive_identity("web01", "web01.corp.example.com"), {"warn=fqdn_consistent = 0"}, ok_response), PB::Common::ResultCode::OK)
      << join_lines(ok_response);
}

// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_hostname.hpp"

#include <gtest/gtest.h>

using hostname_check::host_identity;
using hostname_check::join_status_to_string;

namespace {

std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

host_identity domain_host() {
  host_identity h;
  h.hostname = "WEB01";
  h.dns_hostname = "web01";
  h.domain = "corp.example.com";
  h.fqdn = "web01.corp.example.com";
  h.join = "domain";
  h.join_name = "corp.example.com";
  h.recompute();
  return h;
}

host_identity workgroup_host() {
  host_identity h;
  h.hostname = "MYPC";
  h.dns_hostname = "mypc";
  h.domain = "";
  h.fqdn = "mypc";
  h.join = "workgroup";
  h.join_name = "WORKGROUP";
  h.recompute();
  return h;
}

PB::Common::ResultCode run_check(const host_identity &info, const std::vector<std::string> &args, PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_hostname");
  for (const std::string &a : args) request.add_arguments(a);
  hostname_check::check_from(request, &response, info);
  return response.result();
}

}  // namespace

// --- pure helpers ---------------------------------------------------------------

TEST(CheckHostname, JoinStatusMapping) {
  EXPECT_EQ(join_status_to_string(0), "unknown");
  EXPECT_EQ(join_status_to_string(1), "standalone");
  EXPECT_EQ(join_status_to_string(2), "workgroup");
  EXPECT_EQ(join_status_to_string(3), "domain");
  EXPECT_EQ(join_status_to_string(42), "unknown");
}

TEST(CheckHostname, ConsistentDomainJoinedIdentity) {
  const host_identity h = domain_host();
  EXPECT_TRUE(h.fqdn_consistent);
  EXPECT_TRUE(h.netbios_matches_dns);
}

TEST(CheckHostname, BlankDomainWithBareFqdnIsConsistent) {
  // A workgroup host with no DNS suffix: fqdn == hostname is not drift.
  const host_identity h = workgroup_host();
  EXPECT_TRUE(h.fqdn_consistent);
  EXPECT_TRUE(h.netbios_matches_dns);
}

TEST(CheckHostname, FqdnSuffixDriftIsDetected) {
  host_identity h = domain_host();
  h.fqdn = "web01.old.example.com";  // FQDN kept an old suffix
  h.recompute();
  EXPECT_FALSE(h.fqdn_consistent);
}

TEST(CheckHostname, NetbiosComparesAgainstTruncatedDnsName) {
  host_identity h;
  h.hostname = "VERYLONGHOSTNAM";  // NetBIOS caps at 15 chars
  h.dns_hostname = "verylonghostname01";
  h.domain = "corp.example.com";
  h.fqdn = "verylonghostname01.corp.example.com";
  h.recompute();
  EXPECT_TRUE(h.netbios_matches_dns);  // truncation alone is not a mismatch
  EXPECT_TRUE(h.fqdn_consistent);

  h.hostname = "OLDNAME";  // renamed in DNS but NetBIOS never followed
  h.recompute();
  EXPECT_FALSE(h.netbios_matches_dns);
}

TEST(CheckHostname, CaseDifferencesAreNotDrift) {
  host_identity h = domain_host();
  h.fqdn = "WEB01.CORP.EXAMPLE.COM";
  h.recompute();
  EXPECT_TRUE(h.fqdn_consistent);
}

// --- rendering / thresholds ------------------------------------------------------

TEST(CheckHostname, DefaultIsOkIdentityLine) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(domain_host(), {}, response), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_NE(join_lines(response).find("WEB01 (web01.corp.example.com), domain=corp.example.com"), std::string::npos) << join_lines(response);
}

TEST(CheckHostname, WorkgroupTripsJoinPolicy) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(workgroup_host(), {"crit=join != 'domain'"}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  PB::Commands::QueryResponseMessage::Response ok_response;
  EXPECT_EQ(run_check(domain_host(), {"crit=join != 'domain'"}, ok_response), PB::Common::ResultCode::OK) << join_lines(ok_response);
}

TEST(CheckHostname, PinnedDomainMismatchIsCritical) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(domain_host(), {"crit=domain != 'other.example.com'"}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
}

TEST(CheckHostname, ConsistencyFlagsAreThresholdable) {
  host_identity drifted = domain_host();
  drifted.fqdn = "web01.old.example.com";
  drifted.recompute();
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(drifted, {"warn=fqdn_consistent = 0 or netbios_matches_dns = 0"}, response), PB::Common::ResultCode::WARNING) << join_lines(response);
  PB::Commands::QueryResponseMessage::Response ok_response;
  EXPECT_EQ(run_check(domain_host(), {"warn=fqdn_consistent = 0 or netbios_matches_dns = 0"}, ok_response), PB::Common::ResultCode::OK)
      << join_lines(ok_response);
}

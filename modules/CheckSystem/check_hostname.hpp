// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>

namespace hostname_check {

// Host identity: names, DNS suffix and domain-join state. The interesting
// alerts are pinned expectations — join/domain/hostname drifted from what the
// site expects — and the two derived consistency flags that catch the classic
// silent-auth-failure modes (FQDN not matching hostname+suffix, NetBIOS name
// diverged from the DNS hostname after a rename or re-image).
struct host_identity {
  std::string hostname;      // NetBIOS computer name (max 15 chars)
  std::string dns_hostname;  // DNS hostname (local label)
  std::string domain;        // primary DNS suffix (empty when none)
  std::string fqdn;          // fully qualified DNS name
  std::string join;          // 'domain', 'workgroup', 'standalone' or 'unknown'
  std::string join_name;     // the joined domain or workgroup name

  // Derived by recompute():
  bool fqdn_consistent;      // fqdn == dns_hostname[.domain] (case-insensitive)
  bool netbios_matches_dns;  // NetBIOS name == first 15 chars of the DNS hostname

  host_identity() : fqdn_consistent(false), netbios_matches_dns(false) {}

  void recompute();

  std::string get_hostname() const { return hostname; }
  std::string get_dns_hostname() const { return dns_hostname; }
  std::string get_domain() const { return domain; }
  std::string get_fqdn() const { return fqdn; }
  std::string get_join() const { return join; }
  std::string get_join_name() const { return join_name; }
  long long get_fqdn_consistent() const { return fqdn_consistent ? 1 : 0; }
  long long get_netbios_matches_dns() const { return netbios_matches_dns ? 1 : 0; }

  std::string show() const { return fqdn.empty() ? hostname : fqdn; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<host_identity> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<host_identity, filter_obj_handler> filter_type;

// Map a NETSETUP_JOIN_STATUS value to the join keyword values. Pure, exposed
// for unit tests (0=unknown, 1=standalone, 2=workgroup, 3=domain).
std::string join_status_to_string(long long status);

// Testable core: renders / thresholds a pre-gathered identity row.
void check_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response, const host_identity &info);

// Gather via GetComputerNameEx + NetGetJoinInformation (no WMI/COM involved).
host_identity gather_identity();

// Live check.
void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace hostname_check

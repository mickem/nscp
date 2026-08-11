// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#ifndef NSCP_CHECK_HOSTNAME_H
#define NSCP_CHECK_HOSTNAME_H

#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>

namespace hostname_check {

// Host identity: hostname, canonical FQDN and the derived DNS domain. The unix
// counterpart to the Windows check_hostname; the shared keywords (hostname,
// fqdn, domain, fqdn_consistent) carry the same meaning on both platforms.
// Windows adds join/join_name (domain membership), which has no clean Linux
// equivalent and is absent here.
struct host_identity {
  std::string hostname;  // gethostname()
  std::string fqdn;      // canonical name from the resolver; falls back to hostname
  std::string domain;    // fqdn with the first label removed (empty when none)
  bool fqdn_consistent;  // fqdn equals, or starts with, the configured hostname

  host_identity() : fqdn_consistent(false) {}

  std::string get_hostname() const { return hostname; }
  std::string get_fqdn() const { return fqdn; }
  std::string get_domain() const { return domain; }
  long long get_fqdn_consistent() const { return fqdn_consistent ? 1 : 0; }

  std::string show() const { return fqdn.empty() ? hostname : fqdn; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<host_identity> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<host_identity, filter_obj_handler> filter;

// Build the identity row from the configured hostname and the resolver's
// canonical name (empty when resolution failed). Pure, exposed for unit
// tests: derives domain and the consistency flag.
host_identity derive_identity(const std::string &hostname, const std::string &canonical);

// Testable core: renders / thresholds a pre-gathered identity row.
void check_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response, const host_identity &info);

// Live check: gethostname() + getaddrinfo(AI_CANONNAME).
void check_hostname(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace hostname_check

#endif  // NSCP_CHECK_HOSTNAME_H

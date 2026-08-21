// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_hostname.hpp"

// Windows.h must precede lm.h; the capital W keeps clang-format's
// case-sensitive include sort from breaking that order.
#include <Windows.h>
#include <lm.h>

#include <boost/algorithm/string.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <str/utf8.hpp>
#include <vector>

namespace hostname_check {

using parsers::where::type_bool;

void host_identity::recompute() {
  // With no DNS suffix the FQDN legitimately equals the bare hostname; that is
  // consistent, not drift.
  fqdn_consistent = domain.empty() ? boost::iequals(fqdn, dns_hostname) : boost::iequals(fqdn, dns_hostname + "." + domain);
  // NetBIOS names are capped at 15 characters, so compare against the
  // truncated DNS hostname (a longer DNS name is not by itself a mismatch).
  netbios_matches_dns = boost::iequals(hostname, dns_hostname.substr(0, 15));
}

std::string join_status_to_string(const long long status) {
  switch (status) {
    case 1:  // NetSetupUnjoined
      return "standalone";
    case 2:  // NetSetupWorkgroupName
      return "workgroup";
    case 3:  // NetSetupDomainName
      return "domain";
    default:  // NetSetupUnknownStatus
      return "unknown";
  }
}

filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_string_var("hostname", &host_identity::get_hostname, "NetBIOS computer name (max 15 characters)")
      .add_string_var("dns_hostname", &host_identity::get_dns_hostname, "DNS hostname (the local label)")
      .add_string_var("domain", &host_identity::get_domain, "Primary DNS suffix (empty when none is configured)")
      .add_string_var("fqdn", &host_identity::get_fqdn, "Fully qualified DNS name")
      .add_string_var("join", &host_identity::get_join, "Join state: domain, workgroup, standalone or unknown")
      .add_string_var("join_name", &host_identity::get_join_name, "The joined domain or workgroup name");
  registry_.add_int_var("fqdn_consistent", type_bool, &host_identity::get_fqdn_consistent,
                        "True when fqdn == dns_hostname[.domain] (case-insensitive); false flags DNS-suffix drift")
      .add_int_var("netbios_matches_dns", type_bool, &host_identity::get_netbios_matches_dns,
                   "True when the NetBIOS name matches the first 15 characters of the DNS hostname; false flags rename/imaging drift");
  // clang-format on
}

void check_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response, const host_identity &info) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);

  filter_type filter;
  // No default thresholds: whether 'workgroup' is wrong is site policy. Usage
  // is pinned expectations, e.g. crit=join != 'domain', crit=domain !=
  // 'corp.example.com', warn=fqdn_consistent = 0.
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${list}", "${hostname} (${fqdn}), ${join}=${join_name}", "hostname", "", "");

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  const std::shared_ptr<host_identity> record(new host_identity(info));
  filter.match(record);

  filter_helper.post_process(filter);
}

namespace {

std::string get_computer_name(const COMPUTER_NAME_FORMAT format) {
  DWORD size = 0;
  GetComputerNameExW(format, nullptr, &size);  // reports the required size
  if (size == 0) return "";
  std::vector<wchar_t> buf(size + 1, L'\0');
  if (!GetComputerNameExW(format, buf.data(), &size)) return "";
  return utf8::cvt<std::string>(std::wstring(buf.data()));
}

}  // namespace

host_identity gather_identity() {
  host_identity out;
  out.hostname = get_computer_name(ComputerNameNetBIOS);
  out.dns_hostname = get_computer_name(ComputerNameDnsHostname);
  out.domain = get_computer_name(ComputerNameDnsDomain);
  out.fqdn = get_computer_name(ComputerNameDnsFullyQualified);

  LPWSTR name_buf = nullptr;
  NETSETUP_JOIN_STATUS status = NetSetupUnknownStatus;
  if (NetGetJoinInformation(nullptr, &name_buf, &status) == NERR_Success) {
    out.join = join_status_to_string(static_cast<long long>(status));
    if (name_buf != nullptr) out.join_name = utf8::cvt<std::string>(std::wstring(name_buf));
  } else {
    out.join = "unknown";
  }
  if (name_buf != nullptr) NetApiBufferFree(name_buf);

  out.recompute();
  return out;
}

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  try {
    check_from(request, response, gather_identity());
  } catch (const std::exception &e) {
    nscapi::protobuf::functions::set_response_bad(*response, "Failed to read host identity: " + std::string(e.what()));
  }
}

}  // namespace hostname_check

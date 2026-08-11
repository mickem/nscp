// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_hostname.h"

#include <netdb.h>
#include <sys/types.h>
#include <unistd.h>

#include <boost/algorithm/string.hpp>
#include <cstring>
#include <parsers/filter/cli_helper.hpp>

namespace hostname_check {

using parsers::where::type_bool;

filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_string_var("hostname", &host_identity::get_hostname, "Configured hostname (gethostname)")
      .add_string_var("fqdn", &host_identity::get_fqdn, "Canonical fully qualified name from the resolver (hostname when unresolvable)")
      .add_string_var("domain", &host_identity::get_domain, "DNS domain (the FQDN with the first label removed; empty when none)");
  registry_.add_int_var("fqdn_consistent", type_bool, &host_identity::get_fqdn_consistent,
                        "True when the FQDN equals, or starts with, the configured hostname; false flags DNS drift "
                        "(the resolver canonicalises this host under a different name)");
  // clang-format on
}

host_identity derive_identity(const std::string &hostname, const std::string &canonical) {
  host_identity out;
  out.hostname = hostname;
  out.fqdn = canonical.empty() ? hostname : canonical;
  const std::string::size_type dot = out.fqdn.find('.');
  out.domain = dot == std::string::npos ? "" : out.fqdn.substr(dot + 1);
  // A bare FQDN (no suffix configured) is consistent, not drift; drift is the
  // resolver canonicalising this host under a *different* first label.
  out.fqdn_consistent = boost::iequals(out.fqdn, hostname) || boost::istarts_with(out.fqdn, hostname + ".");
  return out;
}

void check_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response, const host_identity &info) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter> filter_helper(request, response, data);

  filter filter_;
  // No default thresholds: usage is pinned expectations, e.g. crit=domain !=
  // 'corp.example.com', crit=hostname != 'web01', warn=fqdn_consistent = 0.
  // Mirrors the Windows check_hostname contract.
  filter_helper.add_options("", "", "", filter_.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${list}", "${hostname} (${fqdn}), domain=${domain}", "hostname", "", "");

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter_)) return;

  const std::shared_ptr<host_identity> record(new host_identity(info));
  filter_.match(record);

  filter_helper.post_process(filter_);
}

void check_hostname(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  char buf[256] = {0};
  if (gethostname(buf, sizeof(buf) - 1) != 0) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Failed to read hostname (gethostname failed)");
  }
  const std::string hostname(buf);

  // Best-effort canonicalisation: containers and hosts without DNS legitimately
  // fail here, in which case the FQDN falls back to the bare hostname.
  std::string canonical;
  struct addrinfo hints;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_flags = AI_CANONNAME;
  struct addrinfo *res = nullptr;
  if (getaddrinfo(hostname.c_str(), nullptr, &hints, &res) == 0 && res != nullptr) {
    if (res->ai_canonname != nullptr) canonical = res->ai_canonname;
    freeaddrinfo(res);
  }

  check_from(request, response, derive_identity(hostname, canonical));
}

}  // namespace hostname_check

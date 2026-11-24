// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_dhcp.hpp"

#include <nscapi/nscapi_program_options.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>

namespace check_dhcp {

void check_dhcp_scopes(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                       const scope_fetcher &fetch) {
  modern_filter::data_container data;
  modern_filter::cli_helper<dhcp_filter::scope_filter> filter_helper(request, response, data);

  dhcp_filter::scope_filter filter;
  // A disabled scope leases nothing, so it cannot run out: only the enabled
  // ones are checked unless the filter says otherwise. A server with none is
  // UNKNOWN - a check that watches for exhaustion on a server that leases
  // nothing is pointed at the wrong host.
  filter_helper.add_options("used_pct > 80", "used_pct > 95", "enabled = 1", filter.get_filter_syntax(), "unknown");
  filter_helper.add_short_threshold("used_pct", modern_filter::shorthand::above, modern_filter::shorthand::unit_percent_number, "80", "95");
  filter_helper.add_syntax("${status}: ${problem_list}", "${scope} (${name}): ${used_pct}% used, ${free} of ${addresses} free", "${scope}",
                           "No enabled DHCP scopes found", "%(status): all %(count) scopes are below their thresholds");

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  std::vector<dhcp_filter::scope_obj_ptr> scopes;
  std::string error;
  if (!fetch(scopes, error)) return nscapi::protobuf::functions::set_response_bad(*response, error);

  for (const dhcp_filter::scope_obj_ptr &scope : scopes) filter.match(scope);
  filter_helper.post_process(filter);
}

void check_dhcp_server(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                       const counter_fetcher &fetch) {
  modern_filter::data_container data;
  modern_filter::cli_helper<dhcp_filter::server_filter> filter_helper(request, response, data);

  dhcp_filter::server_filter filter;
  // A DHCPDECLINE means a client found the address it was offered already in
  // use on the wire: an address conflict, worth a look whenever it happens.
  // NAKs are routine in small numbers (a roaming laptop asking for its old
  // address) and only a sustained rate means a misconfigured scope or relay.
  // The active queue is the overload signal: packets the server has not got
  // to yet.
  filter_helper.add_options("active_queue > 50 or declines > 0 or nacks > 10", "active_queue > 200", "", filter.get_filter_syntax(), "unknown");
  filter_helper.add_short_threshold("active_queue", modern_filter::shorthand::above, modern_filter::shorthand::unit_plain, "50", "200");
  filter_helper.add_short_warning("${active_queue} or declines > 0 or nacks > 10");
  filter_helper.add_syntax("${status}: ${list}",
                           "${discovers} discovers/s, ${offers} offers/s, ${requests} requests/s, ${acks} acks/s, ${nacks} nacks/s, "
                           "${declines} declines/s, ${active_queue} queued",
                           "dhcp", "No DHCP server counters found", "");

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;
  // The traffic is what is worth graphing, thresholded or not.
  for (std::size_t i = 0; i < dhcp_filter::kServerCounterCount; ++i) filter.add_manual_perf(dhcp_filter::kServerCounters[i].keyword);

  std::map<std::string, double> counters;
  std::string error;
  if (!fetch(counters, error)) return nscapi::protobuf::functions::set_response_bad(*response, error);

  filter.match(dhcp_filter::make_server(counters));
  filter_helper.post_process(filter);
}

}  // namespace check_dhcp

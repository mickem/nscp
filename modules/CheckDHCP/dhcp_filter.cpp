// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "dhcp_filter.hpp"

namespace dhcp_filter {

using parsers::where::type_bool;
using parsers::where::type_float;
using parsers::where::type_int;

// --- helpers ----------------------------------------------------------------

std::string format_ipv4(const std::uint32_t address) {
  return std::to_string((address >> 24) & 0xff) + "." + std::to_string((address >> 16) & 0xff) + "." + std::to_string((address >> 8) & 0xff) + "." +
         std::to_string(address & 0xff);
}

int prefix_length(std::uint32_t mask) {
  int bits = 0;
  for (; mask != 0; mask &= mask - 1) ++bits;
  return bits;
}

// DHCP_SUBNET_STATE: DhcpSubnetEnabled = 0, DhcpSubnetDisabled,
// DhcpSubnetEnabledSwitched, DhcpSubnetDisabledSwitched,
// DhcpSubnetInvalidState.
std::string subnet_state_name(const int state) {
  switch (state) {
    case 0:
      return "enabled";
    case 1:
      return "disabled";
    case 2:
      return "enabled_switched";
    case 3:
      return "disabled_switched";
    default:
      return "invalid";
  }
}

bool subnet_state_enabled(const int state) { return state == 0 || state == 2; }

// --- check_dhcp_scopes ------------------------------------------------------

namespace {
double percent(const long long part, const long long whole) {
  if (whole <= 0) return 0.0;
  return static_cast<double>(part) * 100.0 / static_cast<double>(whole);
}
}  // namespace

double scope_obj::get_used_pct() const { return percent(in_use, get_addresses()); }
// An empty scope is 0% used and, by the same token, 0% free: neither side has
// anything to report, and a "free_pct < 10" threshold must not fire on it.
double scope_obj::get_free_pct() const { return percent(available, get_addresses()); }

scope_obj_ptr make_scope(const std::uint32_t subnet, const std::uint32_t mask, const std::string &name, const std::string &comment, const int state,
                         const scope_stats *stats) {
  scope_obj_ptr obj = std::make_shared<scope_obj>();
  obj->subnet = format_ipv4(subnet);
  obj->mask = format_ipv4(mask);
  obj->prefix = prefix_length(mask);
  obj->name = name;
  obj->comment = comment;
  obj->state = subnet_state_name(state);
  obj->enabled = subnet_state_enabled(state);
  if (stats != nullptr) {
    obj->in_use = stats->in_use;
    obj->available = stats->available;
    obj->pending_offers = stats->pending_offers;
    obj->stats_known = true;
  }
  return obj;
}

scope_handler::scope_handler() {
  // clang-format off
  registry_.add_string_var("scope", [](auto obj) { return obj->get_scope(); }, "The scope as network/prefix, for instance 192.168.1.0/24")
      .add_string_var("subnet", [](auto obj) { return obj->subnet; }, "The network address of the scope, for instance 192.168.1.0")
      .add_string_var("mask", [](auto obj) { return obj->mask; }, "The subnet mask of the scope, for instance 255.255.255.0")
      .add_string_var("name", [](auto obj) { return obj->name; }, "The name of the scope as set in the DHCP console")
      .add_string_var("comment", [](auto obj) { return obj->comment; }, "The description of the scope")
      .add_string_var("state", [](auto obj) { return obj->state; },
                      "The scope state: enabled, disabled, enabled_switched, disabled_switched or invalid");
  registry_.add_int_var("prefix", type_int, [](auto obj) { return obj->prefix; }, "The prefix length of the subnet mask (24 for 255.255.255.0)");
  registry_.add_int_var("enabled", type_bool, [](auto obj) { return obj->enabled ? 1LL : 0LL; },
                        "True when the scope hands out addresses (state enabled or enabled_switched)");
  registry_.add_int_var("in_use", type_int, [](auto obj) { return obj->in_use; }, "Addresses of the scope that are leased (reservations in use included)")
      .add_int_perf("", "", "_in_use");
  registry_.add_int_var("free", type_int, [](auto obj) { return obj->available; }, "Addresses of the scope that are still free to lease")
      .add_int_perf("", "", "_free");
  registry_.add_int_var("pending_offers", type_int, [](auto obj) { return obj->pending_offers; },
                        "Addresses offered to a client that has not yet requested them")
      .add_int_perf("", "", "_pending_offers");
  registry_.add_int_var("addresses", type_int, [](auto obj) { return obj->get_addresses(); },
                        "Addresses the scope can lease: in_use plus free (the range minus its exclusions)")
      .add_int_perf("", "", "_addresses");
  registry_.add_numbers("used_pct", type_float, [](auto obj) { return static_cast<long long>(obj->get_used_pct()); },
                        [](auto obj) { return obj->get_used_pct(); }, "Addresses in use as a percentage of the addresses the scope can lease")
      .add_float_perf("%", "", "_used_pct");
  registry_.add_numbers("free_pct", type_float, [](auto obj) { return static_cast<long long>(obj->get_free_pct()); },
                        [](auto obj) { return obj->get_free_pct(); }, "Free addresses as a percentage of the addresses the scope can lease")
      .add_float_perf("%", "", "_free_pct");
  registry_.add_int_var("stats_known", type_bool, [](auto obj) { return obj->stats_known ? 1LL : 0LL; },
                        "False when the server returned no statistics for the scope (it was created while the check ran): the counts are then 0");
  // clang-format on
}

// --- check_dhcp_server ------------------------------------------------------

const server_counter kServerCounters[] = {
    {"Packets Received/sec", "packets_received", &server_obj::packets_received, "Packets received per second"},
    {"Duplicates Dropped/sec", "duplicates_dropped", &server_obj::duplicates_dropped,
     "Duplicate packets dropped per second (a client retransmitting before the server answered)"},
    {"Packets Expired/sec", "packets_expired", &server_obj::packets_expired,
     "Packets per second that expired in the queue before the server got to them (the server is overloaded)"},
    {"Discovers/sec", "discovers", &server_obj::discovers, "DHCPDISCOVER messages received per second"},
    {"Offers/sec", "offers", &server_obj::offers, "DHCPOFFER messages sent per second"},
    {"Requests/sec", "requests", &server_obj::requests, "DHCPREQUEST messages received per second"},
    {"Informs/sec", "informs", &server_obj::informs, "DHCPINFORM messages received per second"},
    {"Acks/sec", "acks", &server_obj::acks, "DHCPACK messages sent per second"},
    {"Nacks/sec", "nacks", &server_obj::nacks, "DHCPNAK messages sent per second (requests the server refused)"},
    {"Declines/sec", "declines", &server_obj::declines,
     "DHCPDECLINE messages received per second (a client found the offered address already in use on the network)"},
    {"Releases/sec", "releases", &server_obj::releases, "DHCPRELEASE messages received per second"},
    {"Active Queue Length", "active_queue", &server_obj::active_queue, "Packets waiting in the server's processing queue"},
    {"Conflict Check Queue Length", "conflict_queue", &server_obj::conflict_queue,
     "Packets waiting for conflict detection (a ping of the address before it is offered)"},
};
const std::size_t kServerCounterCount = sizeof(kServerCounters) / sizeof(kServerCounters[0]);

server_obj_ptr make_server(const std::map<std::string, double> &counters) {
  server_obj_ptr obj = std::make_shared<server_obj>();
  for (std::size_t i = 0; i < kServerCounterCount; ++i) {
    const auto it = counters.find(kServerCounters[i].counter);
    if (it != counters.end()) (*obj).*(kServerCounters[i].field) = it->second;
  }
  return obj;
}

server_handler::server_handler() {
  for (std::size_t i = 0; i < kServerCounterCount; ++i) {
    double server_obj::*field = kServerCounters[i].field;
    const std::string keyword = kServerCounters[i].keyword;
    registry_
        .add_numbers(
            keyword, type_float, [field](auto obj) { return static_cast<long long>((*obj).*field); }, [field](auto obj) { return (*obj).*field; },
            kServerCounters[i].description)
        .add_float_perf("", "", "_" + keyword);
  }
}

}  // namespace dhcp_filter

// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/optional.hpp>
#include <cstdint>
#include <map>
#include <memory>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>

// The records the DHCP Server checks evaluate, and the pure helpers that build
// them. Nothing here touches dhcpsapi or PDH: the Windows readers
// (dhcp_source_win.cpp) fill in the raw numbers and everything derived from
// them is computed here, so the unit test covers it on every platform.
namespace dhcp_filter {

// --- helpers ----------------------------------------------------------------

// A DHCP_IP_ADDRESS is a DWORD in host byte order with the first octet in the
// most significant byte: 0xC0A80100 is 192.168.1.0.
std::string format_ipv4(std::uint32_t address);

// The prefix length of a contiguous netmask (255.255.255.0 -> 24). A mask
// with holes in it (which the DHCP server refuses to create) counts its set
// bits, which is the best a /n notation can say about it.
int prefix_length(std::uint32_t mask);

// DHCP_SUBNET_STATE as the `state` keyword spells it. The "switched" states
// are the ones a scope is in while its address range is being switched over
// to a superscope partner; anything outside the enum reads "invalid".
std::string subnet_state_name(int state);

// Whether a DHCP_SUBNET_STATE value hands out addresses (enabled or
// enabled_switched).
bool subnet_state_enabled(int state);

// --- check_dhcp_scopes ------------------------------------------------------

struct scope_obj {
  std::string subnet;    // network address, "192.168.1.0"
  std::string mask;      // "255.255.255.0"
  long long prefix = 0;  // 24
  std::string name;
  std::string comment;
  std::string state;  // subnet_state_name()
  bool enabled = false;
  long long in_use = 0;
  long long available = 0;  // free addresses ("free" is a CRT debug macro)
  long long pending_offers = 0;
  // False when the server returned no statistics for this scope (it was
  // created between the two reads). The count and percentage keywords then
  // have no value - they render "unknown", match no numeric comparison and
  // write no perfdata - rather than a 0 that would read as an empty scope.
  bool stats_known = false;

  std::string get_scope() const { return subnet + "/" + std::to_string(prefix); }
  long long get_addresses() const { return in_use + available; }
  // A scope with no address range left (every address excluded) has nothing
  // to exhaust: it reads 0% used rather than dividing by zero.
  double get_used_pct() const;
  double get_free_pct() const;

  // The keyword values: empty when the statistics are unknown.
  boost::optional<long long> opt_in_use() const { return known(in_use); }
  boost::optional<long long> opt_free() const { return known(available); }
  boost::optional<long long> opt_pending_offers() const { return known(pending_offers); }
  boost::optional<long long> opt_addresses() const { return known(get_addresses()); }
  boost::optional<double> opt_used_pct() const { return stats_known ? boost::optional<double>(get_used_pct()) : boost::none; }
  boost::optional<double> opt_free_pct() const { return stats_known ? boost::optional<double>(get_free_pct()) : boost::none; }

  // "85% used, 30 of 200 free", or "no statistics" - the default detail line,
  // so a scope without statistics never reads as "0% used".
  std::string get_usage() const;

  std::string show() const { return get_scope(); }

 private:
  boost::optional<long long> known(const long long v) const { return stats_known ? boost::optional<long long>(v) : boost::none; }
};
typedef std::shared_ptr<scope_obj> scope_obj_ptr;

// The raw per-scope numbers the server reports (SCOPE_MIB_INFO), keyed by the
// scope's network address.
struct scope_stats {
  long long in_use = 0;
  long long available = 0;
  long long pending_offers = 0;
};

// Build a scope record from what DhcpGetSubnetInfo and DhcpGetMibInfo
// returned. `stats` is null when the MIB carried no entry for the scope.
scope_obj_ptr make_scope(std::uint32_t subnet, std::uint32_t mask, const std::string &name, const std::string &comment, int state, const scope_stats *stats);

typedef parsers::where::filter_handler_impl<scope_obj_ptr> scope_context;
struct scope_handler : scope_context {
  scope_handler();
};
typedef modern_filter::modern_filters<scope_obj, scope_handler> scope_filter;

// --- check_dhcp_server ------------------------------------------------------

struct server_obj {
  double packets_received = 0;
  double duplicates_dropped = 0;
  double packets_expired = 0;
  double discovers = 0;
  double offers = 0;
  double requests = 0;
  double informs = 0;
  double acks = 0;
  double nacks = 0;
  double declines = 0;
  double releases = 0;
  double active_queue = 0;
  double conflict_queue = 0;

  std::string show() const { return "dhcp"; }
};
typedef std::shared_ptr<server_obj> server_obj_ptr;

// One PDH counter of the "DHCP Server" object: its English name, the keyword
// it becomes, the field it fills and what the keyword means. The one table
// drives the counter list the reader asks for, the record it builds and the
// keywords the check registers, so the three cannot drift apart.
struct server_counter {
  const char *counter;
  const char *keyword;
  double server_obj::*field;
  const char *description;
};
extern const server_counter kServerCounters[];
extern const std::size_t kServerCounterCount;

// Build the server record from a gathered counter map (counter name ->
// value); a counter the host does not have reads 0.
server_obj_ptr make_server(const std::map<std::string, double> &counters);

typedef parsers::where::filter_handler_impl<server_obj_ptr> server_context;
struct server_handler : server_context {
  server_handler();
};
typedef modern_filter::modern_filters<server_obj, server_handler> server_filter;

}  // namespace dhcp_filter

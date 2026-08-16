// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

namespace firewall_rules_filter {

// NET_FW_* values (netfw.h), redefined so the decoding and its tests build on
// every platform.
enum protocol_numbers { protocol_icmpv4 = 1, protocol_tcp = 6, protocol_udp = 17, protocol_icmpv6 = 58, protocol_any = 256 };
enum profile_bits { profile_domain = 0x1, profile_private = 0x2, profile_public = 0x4, profile_all = 0x7fffffff };

// One firewall rule, or - when an expect= name matched nothing - the hole where
// one should have been.
struct filter_obj {
  filter_obj() : enabled(0), present(1), expected(0), any_remote(0), any_port(0), any_any(0), edge_traversal(0), protocol(protocol_any), profile_mask(0) {}

  std::string get_name() const { return name; }
  std::string get_description() const { return description; }
  std::string get_group() const { return group; }
  std::string get_direction() const { return direction; }
  std::string get_action() const { return action; }
  std::string get_protocol() const;
  std::string get_profiles() const;
  std::string get_local_ports() const { return local_ports; }
  std::string get_remote_ports() const { return remote_ports; }
  std::string get_local_addresses() const { return local_addresses; }
  std::string get_remote_addresses() const { return remote_addresses; }
  std::string get_application() const { return application; }
  std::string get_service() const { return service; }
  std::string get_state() const;
  long long get_enabled() const { return enabled; }
  long long get_present() const { return present; }
  long long get_expected() const { return expected; }
  long long get_any_remote() const { return any_remote; }
  long long get_any_port() const { return any_port; }
  long long get_any_any() const { return any_any; }
  long long get_edge_traversal() const { return edge_traversal; }
  std::string show() const { return name; }

  std::string name;
  std::string description;
  std::string group;             // Grouping, e.g. "Remote Desktop"
  std::string direction;         // in / out
  std::string action;            // allow / block
  std::string local_ports;       // "3389", "80,443", "*"
  std::string remote_ports;      //
  std::string local_addresses;   // "*", "10.0.0.0/8", ...
  std::string remote_addresses;  //
  std::string application;       // ApplicationName
  std::string service;           // ServiceName

  long long enabled;         // the rule is switched on
  long long present;         // 0 only for an expect= name that matched no enabled rule
  long long expected;        // 1 when the rule matched an expect= name
  long long any_remote;      // remote addresses are unrestricted
  long long any_port;        // local ports are unrestricted
  long long any_any;         // enabled inbound allow, unrestricted on both - the finding
  long long edge_traversal;  // the rule accepts traffic that traversed NAT
  long long protocol;        // raw protocol number
  long long profile_mask;    // raw profile bit field
};

typedef std::shared_ptr<filter_obj> filter_obj_ptr;
typedef parsers::where::filter_handler_impl<filter_obj_ptr> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

// "tcp", "udp", "icmpv4", "icmpv6", "any", or the number for anything else.
std::string protocol_name(long long protocol);
// "domain,private,public" (or "all" when the rule applies to every profile).
std::string profiles_name(long long profile_mask);
// True when a firewall address/port field means "no restriction": the API writes
// "*" for that, and an empty field is treated the same way.
bool is_unrestricted(const std::string &field);
// Rewrite an empty scope field as "*". The API leaves these empty where the
// firewall UI shows "Any", which would otherwise make `local_ports = '*'` match
// only some of the unrestricted rules.
void normalize_scope(std::string &field);
// Set any_remote / any_port / any_any from the fields already read.
void classify(filter_obj &obj);
// Mark the rules that satisfy each expect= name, and append a not-present row
// for every name that no enabled rule satisfies. Matching is by exact name,
// case insensitively - firewall rule names are localized, so they must be taken
// from the machine being checked.
void apply_expectations(std::vector<filter_obj_ptr> &rules, const std::vector<std::string> &expected);

}  // namespace firewall_rules_filter

namespace firewall_rules_source {
// Windows only (INetFwPolicy2::Rules); the Unix stub sets `error`.
void gather(std::vector<firewall_rules_filter::filter_obj_ptr> &out, std::string &error);
}  // namespace firewall_rules_source

namespace check_firewall_rules_command {
void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}

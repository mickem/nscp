// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <boost/optional.hpp>

#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

namespace firewall_filter {

// One Windows firewall profile (Domain / Private / Public).
struct filter_obj {
  filter_obj() : enabled(0), active(0), policy("local") {}

  std::string get_profile() const { return profile; }
  long long get_enabled() const { return enabled; }
  long long get_active() const { return active; }
  std::string get_inbound() const { return inbound; }
  std::string get_outbound() const { return outbound; }
  std::string get_policy() const { return policy; }
  std::string show() const { return profile; }

  std::string profile;
  long long enabled;  // 1 = enabled, 0 = disabled
  long long active;   // 1 = profile is currently applied to a connected network
  std::string inbound;
  std::string outbound;
  std::string policy;  // "local" or "group policy" (any setting enforced by GP)
};

typedef std::shared_ptr<filter_obj> filter_obj_ptr;

typedef parsers::where::filter_handler_impl<filter_obj_ptr> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace firewall_filter

namespace firewall_source {
// Populate the firewall profiles for the current platform. On Unix this is a
// stub that sets `error` (there is no Windows-profile firewall model on Linux).
void gather(std::vector<firewall_filter::filter_obj_ptr> &out, std::string &error);

// Group Policy resultant values for one profile, as the policy engine writes
// them under HKLM\SOFTWARE\Policies\Microsoft\WindowsFirewall\<Profile>Profile.
// An unset field is "not configured" in GP: the local value stays in effect.
struct policy_override {
  boost::optional<bool> enabled;         // EnableFirewall
  boost::optional<bool> inbound_block;   // DefaultInboundAction (1 = block)
  boost::optional<bool> outbound_block;  // DefaultOutboundAction (1 = block)
};

// Overlay the GP resultant values on the local-store values: the firewall
// service enforces the GP value when one is set, but INetFwPolicy2 keeps
// reporting the local store, so without this the check reports the pre-policy
// state (issue #1351).
inline void apply_policy_override(firewall_filter::filter_obj &obj, const policy_override &gp) {
  if (!gp.enabled && !gp.inbound_block && !gp.outbound_block) return;
  obj.policy = "group policy";
  if (gp.enabled) obj.enabled = *gp.enabled ? 1 : 0;
  if (gp.inbound_block) obj.inbound = *gp.inbound_block ? "block" : "allow";
  if (gp.outbound_block) obj.outbound = *gp.outbound_block ? "block" : "allow";
}
}  // namespace firewall_source

namespace check_firewall_command {
void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}

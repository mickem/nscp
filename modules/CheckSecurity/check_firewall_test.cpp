// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The Group Policy overlay of check_firewall (issue #1351): a value set in GP
// is what the firewall service enforces, so it must win over the local-store
// value INetFwPolicy2 reports; "not configured" must leave the local value.

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>

#include "check_firewall.hpp"

// Normally provided by NSC_WRAP_DLL() in the auto-generated module.cpp; in the
// test binary there is no generated module, so define the singleton here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

firewall_filter::filter_obj local_profile(long long enabled) {
  firewall_filter::filter_obj obj;
  obj.profile = "Domain";
  obj.enabled = enabled;
  obj.inbound = "block";
  obj.outbound = "allow";
  return obj;
}

}  // namespace

TEST(check_firewall, gp_disabled_overrides_locally_enabled) {
  firewall_filter::filter_obj obj = local_profile(1);
  firewall_source::policy_override gp;
  gp.enabled = false;
  firewall_source::apply_policy_override(obj, gp);
  EXPECT_EQ(0, obj.enabled);
  EXPECT_EQ("group policy", obj.get_policy());
}

TEST(check_firewall, gp_enabled_overrides_locally_disabled) {
  firewall_filter::filter_obj obj = local_profile(0);
  firewall_source::policy_override gp;
  gp.enabled = true;
  firewall_source::apply_policy_override(obj, gp);
  EXPECT_EQ(1, obj.enabled);
  EXPECT_EQ("group policy", obj.get_policy());
}

TEST(check_firewall, gp_not_configured_keeps_local_values) {
  firewall_filter::filter_obj obj = local_profile(0);
  firewall_source::apply_policy_override(obj, firewall_source::policy_override());
  EXPECT_EQ(0, obj.enabled);
  EXPECT_EQ("block", obj.get_inbound());
  EXPECT_EQ("allow", obj.get_outbound());
  EXPECT_EQ("local", obj.get_policy());
}

TEST(check_firewall, gp_default_actions_map_to_allow_and_block) {
  firewall_filter::filter_obj obj = local_profile(1);
  firewall_source::policy_override gp;
  gp.inbound_block = false;  // DefaultInboundAction = 0 (allow)
  gp.outbound_block = true;  // DefaultOutboundAction = 1 (block)
  firewall_source::apply_policy_override(obj, gp);
  EXPECT_EQ("allow", obj.get_inbound());
  EXPECT_EQ("block", obj.get_outbound());
}

TEST(check_firewall, gp_partial_override_keeps_unconfigured_local_values) {
  firewall_filter::filter_obj obj = local_profile(1);
  firewall_source::policy_override gp;
  gp.inbound_block = true;
  firewall_source::apply_policy_override(obj, gp);
  EXPECT_EQ(1, obj.enabled);
  EXPECT_EQ("allow", obj.get_outbound());
  EXPECT_EQ("group policy", obj.get_policy());
}

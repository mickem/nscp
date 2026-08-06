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

TEST(check_firewall, legacy_standard_profile_used_when_wfas_key_is_empty) {
  firewall_source::policy_override modern;
  firewall_source::policy_override legacy;
  legacy.enabled = false;
  const firewall_source::policy_override picked = firewall_source::pick_policy_override(modern, legacy);
  ASSERT_TRUE(picked.enabled);
  EXPECT_FALSE(*picked.enabled);
}

TEST(check_firewall, wfas_values_take_precedence_over_legacy_as_a_whole) {
  // Any modern value present means the legacy key is ignored entirely,
  // not merged per-value: legacy enabled=false must not leak through.
  firewall_source::policy_override modern;
  modern.inbound_block = true;
  firewall_source::policy_override legacy;
  legacy.enabled = false;
  legacy.outbound_block = true;
  const firewall_source::policy_override picked = firewall_source::pick_policy_override(modern, legacy);
  EXPECT_FALSE(picked.enabled);
  ASSERT_TRUE(picked.inbound_block);
  EXPECT_TRUE(*picked.inbound_block);
  EXPECT_FALSE(picked.outbound_block);
}

TEST(check_firewall, neither_policy_store_configured_keeps_local_state) {
  firewall_filter::filter_obj obj = local_profile(1);
  const firewall_source::policy_override picked = firewall_source::pick_policy_override(firewall_source::policy_override(), firewall_source::policy_override());
  firewall_source::apply_policy_override(obj, picked);
  EXPECT_EQ(1, obj.enabled);
  EXPECT_EQ("local", obj.get_policy());
}

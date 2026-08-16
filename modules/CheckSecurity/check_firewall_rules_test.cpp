// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// check_firewall_rules answers two questions the per-profile check cannot: is
// the rule I depend on still there and switched on, and is there an inbound
// allow rule that restricts nothing. COM only supplies the fields; this is the
// part that decides what they mean.

#include <gtest/gtest.h>

#include "check_firewall_rules.hpp"

// The data source is the Windows half of the check and is exercised by the
// integration suite instead; the command object still needs it to link.
namespace firewall_rules_source {
void gather(std::vector<firewall_rules_filter::filter_obj_ptr> & /*out*/, std::string &error) { error = "not available in the unit test"; }
}  // namespace firewall_rules_source

namespace {

using firewall_rules_filter::filter_obj;
using firewall_rules_filter::filter_obj_ptr;

filter_obj_ptr make_rule(const std::string &name, const std::string &direction, const std::string &action, const std::string &local_ports,
                         const std::string &remote_addresses, const long long enabled = 1) {
  auto rule = std::make_shared<filter_obj>();
  rule->name = name;
  rule->direction = direction;
  rule->action = action;
  rule->local_ports = local_ports;
  rule->remote_addresses = remote_addresses;
  rule->enabled = enabled;
  rule->protocol = firewall_rules_filter::protocol_tcp;
  rule->profile_mask = firewall_rules_filter::profile_domain | firewall_rules_filter::profile_private;
  firewall_rules_filter::classify(*rule);
  return rule;
}

}  // namespace

TEST(check_firewall_rules, protocol_names) {
  EXPECT_EQ("tcp", firewall_rules_filter::protocol_name(6));
  EXPECT_EQ("udp", firewall_rules_filter::protocol_name(17));
  EXPECT_EQ("icmpv4", firewall_rules_filter::protocol_name(1));
  EXPECT_EQ("icmpv6", firewall_rules_filter::protocol_name(58));
  EXPECT_EQ("any", firewall_rules_filter::protocol_name(256));
  // Anything else keeps its number rather than being flattened to "unknown".
  EXPECT_EQ("47", firewall_rules_filter::protocol_name(47));
}

TEST(check_firewall_rules, profile_names) {
  EXPECT_EQ("all", firewall_rules_filter::profiles_name(firewall_rules_filter::profile_all));
  EXPECT_EQ("domain", firewall_rules_filter::profiles_name(firewall_rules_filter::profile_domain));
  EXPECT_EQ("private,public", firewall_rules_filter::profiles_name(firewall_rules_filter::profile_private | firewall_rules_filter::profile_public));
  EXPECT_EQ("domain,private,public",
            firewall_rules_filter::profiles_name(firewall_rules_filter::profile_domain | firewall_rules_filter::profile_private |
                                                 firewall_rules_filter::profile_public));
  EXPECT_EQ("none", firewall_rules_filter::profiles_name(0));
}

TEST(check_firewall_rules, unrestricted_fields) {
  EXPECT_TRUE(firewall_rules_filter::is_unrestricted("*"));
  EXPECT_TRUE(firewall_rules_filter::is_unrestricted(""));
  EXPECT_TRUE(firewall_rules_filter::is_unrestricted("  * "));
  EXPECT_FALSE(firewall_rules_filter::is_unrestricted("3389"));
  EXPECT_FALSE(firewall_rules_filter::is_unrestricted("10.0.0.0/8"));
}

TEST(check_firewall_rules, a_scoped_inbound_allow_is_not_a_finding) {
  const filter_obj_ptr rule = make_rule("RDP from the jump host", "in", "allow", "3389", "10.0.0.5");

  EXPECT_EQ(0, rule->get_any_remote());
  EXPECT_EQ(0, rule->get_any_port());
  EXPECT_EQ(0, rule->get_any_any());
}

TEST(check_firewall_rules, an_inbound_allow_open_to_everything_is_a_finding) {
  const filter_obj_ptr rule = make_rule("Something careless", "in", "allow", "*", "*");

  EXPECT_EQ(1, rule->get_any_remote());
  EXPECT_EQ(1, rule->get_any_port());
  EXPECT_EQ(1, rule->get_any_any());
  EXPECT_NE(std::string::npos, rule->get_state().find("(unrestricted)"));
}

TEST(check_firewall_rules, one_restriction_is_enough_to_clear_any_any) {
  // Open port range from one address, or any address on one port: broad, but
  // not the "anything from anywhere" rule this flags.
  EXPECT_EQ(0, make_rule("Any port from one host", "in", "allow", "*", "10.0.0.5")->get_any_any());
  EXPECT_EQ(0, make_rule("One port from anywhere", "in", "allow", "443", "*")->get_any_any());
}

TEST(check_firewall_rules, outbound_and_block_rules_are_never_any_any) {
  // Outbound is unrestricted by default on Windows, and a wide *block* is the
  // opposite of a finding; flagging either would flag every machine.
  EXPECT_EQ(0, make_rule("Outbound everything", "out", "allow", "*", "*")->get_any_any());
  EXPECT_EQ(0, make_rule("Block everything", "in", "block", "*", "*")->get_any_any());
}

TEST(check_firewall_rules, a_disabled_wide_open_rule_is_not_in_effect) {
  EXPECT_EQ(0, make_rule("Disabled and careless", "in", "allow", "*", "*", 0)->get_any_any());
}

TEST(check_firewall_rules, an_expected_rule_that_exists_and_is_enabled_is_satisfied) {
  std::vector<filter_obj_ptr> rules = {make_rule("Remote Desktop - User Mode (TCP-In)", "in", "allow", "3389", "*")};
  firewall_rules_filter::apply_expectations(rules, {"Remote Desktop - User Mode (TCP-In)"});

  ASSERT_EQ(1u, rules.size());  // no hole was appended
  EXPECT_EQ(1, rules[0]->get_present());
  EXPECT_EQ(1, rules[0]->get_expected());
}

TEST(check_firewall_rules, expectations_match_the_name_case_insensitively) {
  std::vector<filter_obj_ptr> rules = {make_rule("Remote Desktop - User Mode (TCP-In)", "in", "allow", "3389", "*")};
  firewall_rules_filter::apply_expectations(rules, {"remote desktop - USER MODE (tcp-in)"});

  ASSERT_EQ(1u, rules.size());
  EXPECT_EQ(1, rules[0]->get_expected());
}

TEST(check_firewall_rules, a_missing_expected_rule_becomes_a_row_of_its_own) {
  std::vector<filter_obj_ptr> rules = {make_rule("Something else", "in", "allow", "80", "*")};
  firewall_rules_filter::apply_expectations(rules, {"Remote Desktop - User Mode (TCP-In)"});

  ASSERT_EQ(2u, rules.size());
  const filter_obj_ptr &hole = rules.back();
  EXPECT_EQ("Remote Desktop - User Mode (TCP-In)", hole->get_name());
  EXPECT_EQ(0, hole->get_present());
  EXPECT_EQ(1, hole->get_expected());
  EXPECT_EQ("not in effect: no rule with this name", hole->get_state());
}

TEST(check_firewall_rules, an_expected_rule_that_is_only_disabled_still_fails_and_says_so) {
  // The rule was not deleted, someone switched it off - a different fix, so a
  // different message, but the same verdict.
  std::vector<filter_obj_ptr> rules = {make_rule("Remote Desktop - User Mode (TCP-In)", "in", "allow", "3389", "*", 0)};
  firewall_rules_filter::apply_expectations(rules, {"Remote Desktop - User Mode (TCP-In)"});

  ASSERT_EQ(2u, rules.size());
  EXPECT_EQ(0, rules.back()->get_present());
  EXPECT_EQ("not in effect: the rule exists but is disabled", rules.back()->get_state());
}

TEST(check_firewall_rules, one_enabled_copy_satisfies_an_expectation_with_duplicates) {
  // Windows happily holds several rules with the same name; one enabled copy is
  // enough for the traffic to be allowed.
  std::vector<filter_obj_ptr> rules = {make_rule("File and Printer Sharing (SMB-In)", "in", "allow", "445", "*", 0),
                                       make_rule("File and Printer Sharing (SMB-In)", "in", "allow", "445", "*", 1)};
  firewall_rules_filter::apply_expectations(rules, {"File and Printer Sharing (SMB-In)"});

  ASSERT_EQ(2u, rules.size());  // nothing appended
  EXPECT_EQ(1, rules[0]->get_expected());
  EXPECT_EQ(1, rules[1]->get_expected());
}

TEST(check_firewall_rules, the_state_summarises_what_a_rule_does) {
  const filter_obj_ptr rule = make_rule("RDP from the jump host", "in", "allow", "3389", "10.0.0.5");
  EXPECT_EQ("in allow tcp port 3389 from 10.0.0.5, domain,private", rule->get_state());

  const filter_obj_ptr disabled = make_rule("RDP from the jump host", "in", "allow", "3389", "10.0.0.5", 0);
  EXPECT_EQ("in allow tcp port 3389 from 10.0.0.5, domain,private, disabled", disabled->get_state());
}

TEST(check_firewall_rules, empty_scope_fields_read_as_any) {
  // A rule for every protocol has no ports to name, so the API leaves the field
  // empty where the firewall UI says "Any". Both must behave the same, and both
  // must be matchable as '*'.
  auto rule = std::make_shared<filter_obj>();
  rule->name = "Protocol-agnostic";
  rule->direction = "in";
  rule->action = "allow";
  rule->enabled = 1;
  rule->local_ports = "";
  rule->remote_addresses = "";
  rule->local_addresses = "  ";
  firewall_rules_filter::classify(*rule);

  EXPECT_EQ("*", rule->get_local_ports());
  EXPECT_EQ("*", rule->get_remote_addresses());
  EXPECT_EQ("*", rule->get_local_addresses());
  EXPECT_EQ(1, rule->get_any_port());
  EXPECT_EQ(1, rule->get_any_remote());
  EXPECT_EQ(1, rule->get_any_any());
}

TEST(check_firewall_rules, a_duplicated_expect_name_yields_one_hole_that_tells_the_truth) {
  // expect= is repeatable; the same name twice must not let the second pass
  // find the hole the first appended and report "exists but is disabled" for a
  // rule that does not exist at all.
  std::vector<filter_obj_ptr> rules = {make_rule("Something else", "in", "allow", "80", "*")};
  firewall_rules_filter::apply_expectations(rules, {"Ghost rule", "Ghost rule"});

  ASSERT_EQ(2u, rules.size());  // exactly one hole
  EXPECT_EQ("not in effect: no rule with this name", rules.back()->get_state());
}

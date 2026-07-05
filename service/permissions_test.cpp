// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "permissions.hpp"

#include <gtest/gtest.h>

using nsclient::core::permissions;

// ===== disabled / no-rules stance =========================================

TEST(Permissions, disabled_allows_everything) {
  permissions p;
  // enabled defaults to false - the rollout default. No rules registered.
  EXPECT_TRUE(p.is_allowed("WEBServer:nobody", "CheckSystem.check_cpu"));
  EXPECT_TRUE(p.is_allowed("*", "*"));
}

TEST(Permissions, enabled_with_no_rules_denies_everything) {
  // Strict allow-list: with no rules, nothing is allowed. The
  // configurable `default = allow|deny` knob is gone - in an allow-only
  // rule world, default=allow was a no-op that just confused operators.
  permissions p;
  p.set_enabled(true);
  EXPECT_FALSE(p.is_allowed("WEBServer:nobody", "CheckSystem.check_cpu"));
  EXPECT_FALSE(p.is_allowed("*", "*"));
}

// ===== basic rule matching ================================================

TEST(Permissions, exact_subject_exact_object) {
  permissions p;
  p.set_enabled(true);
  p.add_rule("WEBServer:admin", "CheckSystem.check_cpu");
  EXPECT_TRUE(p.is_allowed("WEBServer:admin", "CheckSystem.check_cpu"));
  EXPECT_FALSE(p.is_allowed("WEBServer:admin", "CheckSystem.check_mem"));
  EXPECT_FALSE(p.is_allowed("WEBServer:other", "CheckSystem.check_cpu"));
}

TEST(Permissions, comma_separated_objects) {
  permissions p;
  p.set_enabled(true);
  p.add_rule("NRPEServer", "CheckHelpers.*, CheckSystem.check_cpu, CheckSystem.check_drivesize");
  EXPECT_TRUE(p.is_allowed("NRPEServer", "CheckHelpers.check_ok"));
  EXPECT_TRUE(p.is_allowed("NRPEServer", "CheckSystem.check_cpu"));
  EXPECT_TRUE(p.is_allowed("NRPEServer", "CheckSystem.check_drivesize"));
  EXPECT_FALSE(p.is_allowed("NRPEServer", "CheckSystem.check_mem"));
}

TEST(Permissions, whitespace_around_commas_is_ignored) {
  permissions p;
  p.set_enabled(true);
  p.add_rule("X", "  A.foo  ,B.bar  ,   C.baz");
  EXPECT_TRUE(p.is_allowed("X", "A.foo"));
  EXPECT_TRUE(p.is_allowed("X", "B.bar"));
  EXPECT_TRUE(p.is_allowed("X", "C.baz"));
}

// ===== glob semantics =====================================================

TEST(Permissions, star_matches_everything) {
  permissions p;
  p.set_enabled(true);
  p.add_rule("WEBServer:admin", "*");
  EXPECT_TRUE(p.is_allowed("WEBServer:admin", "CheckSystem.check_cpu"));
  EXPECT_TRUE(p.is_allowed("WEBServer:admin", "Anything.anywhere"));
}

TEST(Permissions, module_dot_star_matches_module) {
  permissions p;
  p.set_enabled(true);
  p.add_rule("X", "CheckHelpers.*");
  EXPECT_TRUE(p.is_allowed("X", "CheckHelpers.check_ok"));
  EXPECT_TRUE(p.is_allowed("X", "CheckHelpers.check_warning"));
  EXPECT_FALSE(p.is_allowed("X", "CheckSystem.check_cpu"));
}

TEST(Permissions, question_mark_matches_one_char) {
  permissions p;
  p.set_enabled(true);
  p.add_rule("X", "A.check_?");
  EXPECT_TRUE(p.is_allowed("X", "A.check_a"));
  EXPECT_TRUE(p.is_allowed("X", "A.check_x"));
  EXPECT_FALSE(p.is_allowed("X", "A.check_ab"));
  EXPECT_FALSE(p.is_allowed("X", "A.check_"));
}

TEST(Permissions, bare_command_matches_any_module) {
  permissions p;
  p.set_enabled(true);
  p.add_rule("X", "check_cpu");
  EXPECT_TRUE(p.is_allowed("X", "CheckSystem.check_cpu"));
  EXPECT_TRUE(p.is_allowed("X", "CheckHelpers.check_cpu"));
  EXPECT_FALSE(p.is_allowed("X", "CheckSystem.check_mem"));
}

TEST(Permissions, case_insensitive_matching) {
  permissions p;
  p.set_enabled(true);
  p.add_rule("webserver:Admin", "checksystem.CHECK_CPU");
  EXPECT_TRUE(p.is_allowed("WEBServer:admin", "CheckSystem.check_cpu"));
  EXPECT_TRUE(p.is_allowed("WEBSERVER:ADMIN", "CHECKSYSTEM.CHECK_CPU"));
}

// ===== subject-side semantics =============================================

TEST(Permissions, bare_module_subject_matches_any_principal) {
  permissions p;
  p.set_enabled(true);
  p.add_rule("WEBServer", "CheckSystem.check_cpu");
  EXPECT_TRUE(p.is_allowed("WEBServer", "CheckSystem.check_cpu"));
  EXPECT_TRUE(p.is_allowed("WEBServer:operator", "CheckSystem.check_cpu"));
  EXPECT_TRUE(p.is_allowed("WEBServer:admin", "CheckSystem.check_cpu"));
}

TEST(Permissions, trailing_colon_subject_matches_empty_principal_only) {
  permissions p;
  p.set_enabled(true);
  p.add_rule("WEBServer:", "CheckSystem.check_cpu");
  // "WEBServer" (no principal) should match.
  EXPECT_TRUE(p.is_allowed("WEBServer", "CheckSystem.check_cpu"));
  // "WEBServer:admin" should NOT - the trailing colon means empty principal.
  EXPECT_FALSE(p.is_allowed("WEBServer:admin", "CheckSystem.check_cpu"));
}

TEST(Permissions, colon_star_subject_matches_any_non_empty_principal) {
  permissions p;
  p.set_enabled(true);
  p.add_rule("WEBServer:*", "CheckSystem.check_cpu");
  EXPECT_TRUE(p.is_allowed("WEBServer:admin", "CheckSystem.check_cpu"));
  EXPECT_TRUE(p.is_allowed("WEBServer:operator", "CheckSystem.check_cpu"));
  // Bare "WEBServer" - the glob `WEBServer:*` does NOT match because there's
  // no `:` in the subject. (Standard glob: `*` matches any chars including
  // empty, but the literal `:` in the pattern requires a `:` in the input.)
  EXPECT_FALSE(p.is_allowed("WEBServer", "CheckSystem.check_cpu"));
}

TEST(Permissions, star_subject_matches_anything) {
  permissions p;
  p.set_enabled(true);
  p.add_rule("*", "CheckHelpers.check_ok");
  EXPECT_TRUE(p.is_allowed("WEBServer:admin", "CheckHelpers.check_ok"));
  EXPECT_TRUE(p.is_allowed("NRPEServer", "CheckHelpers.check_ok"));
  EXPECT_TRUE(p.is_allowed("", "CheckHelpers.check_ok"));
  // But still scoped to the matching object.
  EXPECT_FALSE(p.is_allowed("WEBServer:admin", "CheckSystem.check_cpu"));
}

// ===== additive merge =====================================================

TEST(Permissions, multiple_rules_on_same_subject_merge_additively) {
  permissions p;
  p.set_enabled(true);
  p.add_rule("WEBServer:dev", "CheckSystem.check_cpu");
  p.add_rule("WEBServer:dev", "CheckHelpers.*");
  EXPECT_TRUE(p.is_allowed("WEBServer:dev", "CheckSystem.check_cpu"));
  EXPECT_TRUE(p.is_allowed("WEBServer:dev", "CheckHelpers.check_ok"));
  EXPECT_FALSE(p.is_allowed("WEBServer:dev", "CheckSystem.check_mem"));
}

TEST(Permissions, different_subject_rules_dont_bleed) {
  permissions p;
  p.set_enabled(true);
  p.add_rule("WEBServer:dev", "CheckSystem.check_cpu");
  p.add_rule("WEBServer:prod", "CheckHelpers.*");
  EXPECT_TRUE(p.is_allowed("WEBServer:dev", "CheckSystem.check_cpu"));
  EXPECT_FALSE(p.is_allowed("WEBServer:dev", "CheckHelpers.check_ok"));
  EXPECT_FALSE(p.is_allowed("WEBServer:prod", "CheckSystem.check_cpu"));
  EXPECT_TRUE(p.is_allowed("WEBServer:prod", "CheckHelpers.check_ok"));
}

// ===== state / lifecycle ==================================================

TEST(Permissions, clear_rules_resets_policy_table) {
  permissions p;
  p.set_enabled(true);
  p.add_rule("X", "A.b");
  EXPECT_EQ(1u, p.rule_count());
  EXPECT_TRUE(p.is_allowed("X", "A.b"));
  p.clear_rules();
  EXPECT_EQ(0u, p.rule_count());
  EXPECT_FALSE(p.is_allowed("X", "A.b"));
}

TEST(Permissions, clear_rules_keeps_enabled_state) {
  permissions p;
  p.set_enabled(true);
  p.add_rule("X", "A.b");
  p.clear_rules();
  EXPECT_TRUE(p.is_enabled());
}

TEST(Permissions, empty_objects_list_drops_rule) {
  // Defensive: a rule with no object patterns can't authorise anything,
  // so we drop it. Otherwise an admin who types
  //   somesubject =
  // accidentally allows nothing (subject matches, but no obj_patterns to
  // iterate) which the strict allow-list treats as a deny anyway.
  permissions p;
  p.set_enabled(true);
  p.add_rule("X", "");
  EXPECT_EQ(0u, p.rule_count());
}

// ===== make_subject / make_object helpers =================================

TEST(Permissions, make_subject_with_principal) { EXPECT_EQ("WEBServer:admin", permissions::make_subject("WEBServer", "admin")); }

TEST(Permissions, make_subject_without_principal) { EXPECT_EQ("WEBServer", permissions::make_subject("WEBServer", "")); }

TEST(Permissions, make_object_with_module) { EXPECT_EQ("CheckSystem.check_cpu", permissions::make_object("CheckSystem", "check_cpu")); }

TEST(Permissions, make_object_without_module) { EXPECT_EQ("check_cpu", permissions::make_object("", "check_cpu")); }

// ===== allow exec toggle ==================================================
//
// The exec surface (WEB scripts UI, lua/python core:simple_exec, CLI exec)
// is gated by a single global switch, not by the rule table. Tests pin
// the four (enabled, allow_exec) combinations.

TEST(Permissions, exec_allowed_by_default_when_disabled) {
  // Master switch off: exec always allowed (same bypass as is_allowed).
  permissions p;
  EXPECT_TRUE(p.is_exec_allowed());
}

TEST(Permissions, exec_allowed_by_default_when_enabled) {
  // Master switch on, allow_exec at its true default: exec allowed.
  // This is the deliberate rollout choice - flipping `enabled = true`
  // must not silently break exec callers; an operator who wants exec
  // off has to opt in explicitly.
  permissions p;
  p.set_enabled(true);
  EXPECT_TRUE(p.is_exec_allowed());
}

TEST(Permissions, exec_denied_when_enabled_and_toggle_off) {
  permissions p;
  p.set_enabled(true);
  p.set_allow_exec(false);
  EXPECT_FALSE(p.is_exec_allowed());
}

TEST(Permissions, exec_toggle_ignored_when_master_disabled) {
  // The master switch wins: if the policy system is off entirely, exec
  // is allowed regardless of allow_exec. Means an operator can set
  // `allow exec = false` defensively and only have it take effect once
  // they also flip the master switch.
  permissions p;
  EXPECT_FALSE(p.is_enabled());
  p.set_allow_exec(false);
  EXPECT_TRUE(p.is_exec_allowed());
}

TEST(Permissions, exec_toggle_does_not_affect_query_is_allowed) {
  // The toggle gates exec only. Query rules remain in force regardless
  // of allow_exec's value - exec and query policy are independent.
  permissions p;
  p.set_enabled(true);
  p.set_allow_exec(false);
  p.add_rule("WEBServer:admin", "CheckSystem.check_cpu");
  EXPECT_TRUE(p.is_allowed("WEBServer:admin", "CheckSystem.check_cpu"));
  EXPECT_FALSE(p.is_allowed("WEBServer:guest", "CheckSystem.check_cpu"));
}

// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "permissions.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <thread>

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

TEST(Permissions, replace_with_takes_over_rules_and_flags) {
  permissions fresh;
  fresh.set_enabled(true);
  fresh.set_allow_exec(false);
  fresh.set_log_denials(false);
  fresh.set_log_allows(true);
  fresh.add_rule("NRPEServer", "CheckSystem.check_cpu");

  permissions p;
  p.add_rule("WEBServer", "*");
  p.replace_with(fresh);

  EXPECT_TRUE(p.is_enabled());
  EXPECT_FALSE(p.is_exec_allowed());
  EXPECT_FALSE(p.should_log_denials());
  EXPECT_TRUE(p.should_log_allows());
  EXPECT_EQ(1u, p.rule_count());
  EXPECT_TRUE(p.is_allowed("NRPEServer", "CheckSystem.check_cpu"));
  // The old rule is gone, not merged.
  EXPECT_FALSE(p.is_allowed("WEBServer", "CheckSystem.check_cpu"));
}

TEST(Permissions, replace_with_self_is_a_no_op) {
  permissions p;
  p.set_enabled(true);
  p.add_rule("NRPEServer", "*");
  p.replace_with(p);
  EXPECT_EQ(1u, p.rule_count());
  EXPECT_TRUE(p.is_allowed("NRPEServer", "check_cpu"));
}

TEST(Permissions, reader_never_sees_an_empty_table_during_replace) {
  // A settings reload republishes the table while checks keep flowing.
  // Rebuilding in place (clearing the rules, then re-adding them) left a
  // window in which an enabled policy had no rules and denied everything;
  // replace_with must not have one.
  permissions p;
  p.set_enabled(true);
  p.add_rule("NRPEServer", "CheckSystem.*");

  std::atomic<bool> done{false};
  std::atomic<int> denied{0};
  std::thread reader([&] {
    while (!done) {
      if (!p.is_allowed("NRPEServer", "CheckSystem.check_cpu")) ++denied;
    }
  });
  for (int i = 0; i < 2000; ++i) {
    permissions fresh;
    fresh.set_enabled(true);
    fresh.add_rule("NRPEServer", "CheckSystem.*");
    p.replace_with(fresh);
  }
  done = true;
  reader.join();
  EXPECT_EQ(0, denied.load());
}

// ===== failed reload ======================================================

TEST(Permissions, failed_load_before_enabled_keeps_a_disabled_policy_off) {
  // A default install (no [/settings/permissions]) whose reload times out
  // before `enabled` is read must not start refusing every query.
  permissions fresh;
  fresh.complete_failed_load(/*enabled_read=*/false, /*exec_read=*/false, /*previously_enabled=*/false);
  EXPECT_FALSE(fresh.is_enabled());
  EXPECT_TRUE(fresh.is_allowed("NRPEServer", "CheckSystem.check_cpu"));
  EXPECT_TRUE(fresh.is_exec_allowed());
}

TEST(Permissions, failed_load_before_enabled_keeps_an_enabled_policy_closed) {
  permissions fresh;
  fresh.complete_failed_load(/*enabled_read=*/false, /*exec_read=*/false, /*previously_enabled=*/true);
  EXPECT_TRUE(fresh.is_enabled());
  EXPECT_FALSE(fresh.is_allowed("NRPEServer", "CheckSystem.check_cpu"));
  EXPECT_FALSE(fresh.is_exec_allowed());
}

TEST(Permissions, failed_load_honours_an_enabled_value_it_read) {
  // `enabled` was read; it decides, whatever was in force before.
  permissions turned_on;
  turned_on.set_enabled(true);
  turned_on.complete_failed_load(/*enabled_read=*/true, /*exec_read=*/false, /*previously_enabled=*/false);
  EXPECT_TRUE(turned_on.is_enabled());
  EXPECT_FALSE(turned_on.is_exec_allowed());

  permissions turned_off;
  turned_off.complete_failed_load(/*enabled_read=*/true, /*exec_read=*/false, /*previously_enabled=*/true);
  EXPECT_FALSE(turned_off.is_enabled());
}

TEST(Permissions, failed_load_keeps_rules_and_exec_switch_it_read) {
  permissions fresh;
  fresh.set_enabled(true);
  fresh.set_allow_exec(true);
  fresh.add_rule("NRPEServer", "CheckSystem.*");
  fresh.complete_failed_load(/*enabled_read=*/true, /*exec_read=*/true, /*previously_enabled=*/false);
  EXPECT_TRUE(fresh.is_exec_allowed());
  EXPECT_TRUE(fresh.is_allowed("NRPEServer", "CheckSystem.check_cpu"));
  EXPECT_FALSE(fresh.is_allowed("NRPEServer", "CheckDisk.check_drivesize"));
}

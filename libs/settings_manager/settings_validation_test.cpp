// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <settings/settings_validation.hpp>

using settings::inventory;
using settings::inventory_section;
using settings::registry_snapshot;
using settings::validate_inventory;
using settings::validation_finding;
using settings::validation_result;

namespace {

typedef validation_finding::kind kind;
typedef validation_finding::severity severity;

registry_snapshot make_registry() {
  registry_snapshot reg;
  reg.paths["/settings/external scripts"].keys = {"allow arguments", "allow nasty characters", "timeout"};
  reg.paths["/settings/external scripts/scripts"].any_key = true;
  reg.paths["/settings/external scripts/alias"].any_key = true;
  reg.paths["/settings/default"].keys = {"allowed hosts", "password"};
  reg.paths["/settings/log"].keys = {"level", "file name"};
  reg.paths["/modules"].any_key = true;
  return reg;
}

validation_result run(const inventory &inv, const bool complete = false) { return validate_inventory(inv, make_registry(), complete); }

inventory one(const std::string &path, const std::list<std::string> &keys) { return {inventory_section("nsclient.ini", path, keys)}; }

}  // namespace

TEST(settings_validation, registered_configuration_is_clean) {
  inventory inv = {
      inventory_section("nsclient.ini", "/modules", {"CheckExternalScripts", "WhateverModule"}),
      inventory_section("nsclient.ini", "/settings/external scripts", {"allow arguments"}),
      inventory_section("nsclient.ini", "/settings/external scripts/scripts", {"check_cert_expiry_win", "anything"}),
      inventory_section("nsclient.ini", "/settings/default", {"allowed hosts"}),
  };
  EXPECT_TRUE(run(inv).empty());
}

// The case in #647: the sections were written without their leading '/', so
// nothing ever read them and the script was "Unknown command".
TEST(settings_validation, section_without_leading_slash_is_a_warning_with_the_fix) {
  const validation_result r = run(one("settings/external scripts/scripts", {"check_cert_expiry_win"}));
  ASSERT_EQ(r.size(), 1u);
  EXPECT_EQ(r.front().type, kind::relative_path);
  EXPECT_EQ(r.front().level, severity::warning);
  EXPECT_EQ(r.front().suggestion, "/settings/external scripts/scripts");
  EXPECT_EQ(r.front().source, "nsclient.ini");
  EXPECT_NE(r.front().message.find("start with '/'"), std::string::npos) << r.front().message;
}

// The core registered its own section as "settings" for years, so `nscp
// settings --update` left an empty [settings] in every file it wrote. An
// empty section configures nothing: listed, but no reason to warn.
TEST(settings_validation, empty_misplaced_sections_are_info) {
  const validation_result r = run({
      inventory_section("nsclient.ini", "settings", {}),
      inventory_section("nsclient.ini", "/settings/external scripts/scirpts", {}),
  });
  ASSERT_EQ(r.size(), 2u);
  EXPECT_EQ(r.front().type, kind::relative_path);
  EXPECT_EQ(r.front().level, severity::info);
  EXPECT_EQ(r.back().type, kind::unknown_path);
  EXPECT_EQ(r.back().level, severity::info);
}

TEST(settings_validation, keys_above_the_first_section_are_warnings) {
  const validation_result r = run(one("", {"password"}));
  ASSERT_EQ(r.size(), 1u);
  EXPECT_EQ(r.front().type, kind::orphan_key);
  EXPECT_EQ(r.front().level, severity::warning);
  EXPECT_EQ(r.front().key, "password");
}

TEST(settings_validation, paths_and_keys_compare_without_case) { EXPECT_TRUE(run(one("/Settings/External Scripts", {"Allow Arguments"})).empty()); }

TEST(settings_validation, misspelled_key_in_a_registered_section_is_a_warning) {
  const validation_result r = run(one("/settings/external scripts", {"alow arguments"}));
  ASSERT_EQ(r.size(), 1u);
  EXPECT_EQ(r.front().type, kind::unknown_key);
  EXPECT_EQ(r.front().level, severity::warning);
  EXPECT_EQ(r.front().suggestion, "allow arguments");
}

TEST(settings_validation, underscores_for_spaces_count_as_a_near_miss) {
  const validation_result r = run(one("/settings/external scripts", {"allow_arguments"}));
  ASSERT_EQ(r.size(), 1u);
  EXPECT_EQ(r.front().type, kind::unknown_key);
  EXPECT_EQ(r.front().suggestion, "allow arguments");
}

// A shared section carries keys for modules that may not be loaded; a key
// that resembles nothing is only info.
TEST(settings_validation, unrelated_unknown_key_is_info) {
  const validation_result r = run(one("/settings/default", {"inbox"}));
  ASSERT_EQ(r.size(), 1u);
  EXPECT_EQ(r.front().type, kind::unused_key);
  EXPECT_EQ(r.front().level, severity::info);
}

TEST(settings_validation, misspelled_section_is_a_warning) {
  const validation_result r = run(one("/settings/external scripts/scirpts", {"x"}));
  ASSERT_EQ(r.size(), 1u);
  EXPECT_EQ(r.front().type, kind::unknown_path);
  EXPECT_EQ(r.front().level, severity::warning);
  EXPECT_EQ(r.front().suggestion, "/settings/external scripts/scripts");
}

// [/settings/NRPE/server] with NRPEServer disabled: nothing claims it, but
// that is how a disabled module looks, so it must not be a warning.
TEST(settings_validation, section_of_a_module_that_is_not_loaded_is_info) {
  const validation_result r = run(one("/settings/NRPE/server", {"allow arguments"}));
  ASSERT_EQ(r.size(), 1u);
  EXPECT_EQ(r.front().type, kind::unused_path);
  EXPECT_EQ(r.front().level, severity::info);
}

// "lua" is two edits from "log": short names get no typo allowance, or a
// disabled module's section reads as a misspelled core one.
// NSCPClient loaded, NSCAClient disabled with its section left in place - the
// passive monitoring scenario. One edit apart, and both are real sections.
TEST(settings_validation, a_disabled_sibling_module_is_not_a_typo) {
  registry_snapshot reg = make_registry();
  reg.paths["/settings/NSCP/client"].keys = {"channel"};
  const validation_result r = validate_inventory(one("/settings/NSCA/client", {"channel"}), reg, false);
  ASSERT_EQ(r.size(), 1u);
  EXPECT_EQ(r.front().type, kind::unused_path);
  EXPECT_EQ(r.front().level, severity::info);
}

TEST(settings_validation, short_segments_are_not_near_misses) {
  const validation_result r = run(one("/settings/lua", {}));
  ASSERT_EQ(r.size(), 1u);
  EXPECT_EQ(r.front().type, kind::unused_path);
}

TEST(settings_validation, objects_under_a_user_keyed_section_are_not_reported) {
  EXPECT_TRUE(run(one("/settings/external scripts/scripts/my_script", {"command", "whatever"})).empty());
}

TEST(settings_validation, complete_registry_turns_unclaimed_entries_into_warnings) {
  const validation_result r = run(
      {
          inventory_section("nsclient.ini", "/settings/NRPE/server", {"x"}),
          inventory_section("nsclient.ini", "/settings/default", {"inbox"}),
      },
      true);
  ASSERT_EQ(r.size(), 2u);
  for (const validation_finding &f : r) {
    EXPECT_EQ(f.level, severity::warning);
    EXPECT_NE(f.message.find("installed module"), std::string::npos) << f.message;
  }
}

TEST(settings_validation, count_warnings_skips_info) {
  const validation_result r = run({
      inventory_section("a.ini", "settings/x", {"k"}),
      inventory_section("b.ini", "/settings/NRPE/server", {"k"}),
  });
  ASSERT_EQ(r.size(), 2u);
  EXPECT_EQ(settings::count_warnings(r), 1u);
  EXPECT_EQ(r.back().source, "b.ini");
}

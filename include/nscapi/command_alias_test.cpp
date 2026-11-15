// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The alias definitions of CheckExternalScripts (alias::command_object, read
// through the object handler) and CheckHelpers (alias::simple_command_map):
// how a configured command line becomes a command and its arguments, and
// which lines are refused. Linked with proxy_core_test.cpp, which defines the
// plugin singleton the stub core hangs off.

#include <gtest/gtest.h>

#include <list>
#include <memory>
#include <nscapi/command_alias.hpp>
#include <nscapi/settings/proxy.hpp>
#include <nscapi/test_helpers.hpp>
#include <string>

namespace {

const std::string base = "/settings/external scripts/alias";

class CommandAlias : public ::testing::Test {
 protected:
  nscapi::test_helpers::stub_core &core() { return nscapi::test_helpers::stub_core::instance(); }
  void SetUp() override { core().reset(); }

  // An alias as `name = <line>` directly under the alias section.
  alias::command_object one_liner(const std::string &name, const std::string &line) {
    core().set_setting(base, name, line);
    alias::command_object o(name, base);
    o.set_value(line);
    o.read(nscapi::settings_proxy::create(1, nscapi::plugin_singleton->get_core()), true, false);
    return o;
  }
};

std::list<std::string> L(std::initializer_list<std::string> v) { return v; }

}  // namespace

TEST_F(CommandAlias, OneLinerSplitsCommandAndArguments) {
  const auto o = one_liner("Check_Load", "check_cpu \"warn=load > 80\" crit=load>90");
  EXPECT_EQ(o.get_alias(), "check_load");  // aliases are case-insensitive
  EXPECT_EQ(o.command, "check_cpu");
  EXPECT_EQ(o.arguments, L({"warn=load > 80", "crit=load>90"}));
  EXPECT_EQ(o.get_argument(), "warn=load > 80 crit=load>90");
}

TEST_F(CommandAlias, SectionFormReadsTheCommandKey) {
  core().set_setting(base + "/my_alias", "command", "check_memory type=physical");
  alias::command_object o("my_alias", base);
  o.read(nscapi::settings_proxy::create(1, nscapi::plugin_singleton->get_core()), false, false);
  EXPECT_EQ(o.command, "check_memory");
  EXPECT_EQ(o.arguments, L({"type=physical"}));
  EXPECT_TRUE(core().has_key(base + "/my_alias", "command"));
}

TEST_F(CommandAlias, EscapedSpaceAndBackslash) {
  const auto o = one_liner("a", "check_files path=C:\\\\temp\\ dir");
  EXPECT_EQ(o.command, "check_files");
  EXPECT_EQ(o.arguments, L({"path=C:\\temp dir"}));
}

// A line that does not parse cleanly registers nothing - no fallback split
// that might tokenise it differently from the principal parser.
TEST_F(CommandAlias, MalformedLineIsRefused) {
  for (const std::string bad : {std::string("check_files path=C:\\temp"), std::string("check_x \"a\nb\"")}) {
    const auto o = one_liner("bad", bad);
    EXPECT_TRUE(o.command.empty()) << bad;
    EXPECT_TRUE(o.arguments.empty()) << bad;
  }
}

// set_command("") keeps what was there: an empty `command` key under a
// section must not wipe the one-liner value it was cloned from.
TEST_F(CommandAlias, EmptyCommandLeavesTheDefinitionAlone) {
  alias::command_object o("x", base);
  o.set_command("check_ok a b");
  o.set_command("");
  EXPECT_EQ(o.command, "check_ok");
  EXPECT_EQ(o.arguments, L({"a", "b"}));
}

TEST_F(CommandAlias, ToStringShowsCommandAndArguments) {
  alias::command_object o("x", base);
  o.set_command("check_ok a b");
  const std::string s = o.to_string();
  EXPECT_NE(s.find("x[x] = {tpl: "), std::string::npos) << s;
  EXPECT_NE(s.find("command: check_ok, arguments: a,b}"), std::string::npos) << s;
}

TEST_F(CommandAlias, SimpleCommandMap) {
  auto map = alias::make_simple_command_map();
  map.add("Check_Disk", "check_drivesize drive=c: \"warn=used > 80%\"");
  map.add("broken", "check_files path=C:\\temp");
  map.add("blank", "   ");
  EXPECT_EQ(map.size(), 1u);
  EXPECT_FALSE(map.contains("broken"));
  EXPECT_FALSE(map.contains("blank"));
  const auto found = map.find("CHECK_DISK");
  ASSERT_TRUE(found);
  EXPECT_EQ(found.value().name, "check_disk");
  EXPECT_EQ(found.value().command, "check_drivesize");
  EXPECT_EQ(found.value().get_argument(), "drive=c: warn=used > 80%");
  EXPECT_EQ(found.value().get_alias(), "check_disk");
}

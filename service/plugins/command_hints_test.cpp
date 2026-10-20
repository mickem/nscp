// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "command_hints.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <command_module_map.hpp>

using nsclient::core::command_hints::annotate;
using nsclient::core::command_hints::providers_of;
using nsclient::core::command_hints::unknown_commands_message;

namespace {

std::vector<std::string> fake_providers(const std::string &command) {
  if (command == "check_ping") return {"CheckNet"};
  if (command == "check_cpu") return {"CheckSystem", "CheckSystemUnix"};
  return {};
}

}  // namespace

TEST(command_hints, names_the_module_to_enable) {
  EXPECT_EQ("check_ping (provided by module CheckNet, which is not loaded: add \"CheckNet = enabled\" under [/modules] and restart)",
            annotate("check_ping", {"CheckNet"}));
}

TEST(command_hints, no_hint_for_an_unknown_provider) {
  // A script command or an alias: the build knows nothing about it.
  EXPECT_EQ("my_script", annotate("my_script", {}));
}

TEST(command_hints, several_providers) {
  EXPECT_EQ("check_cpu (provided by modules CheckSystem or CheckSystemUnix, none of which is loaded: enable one of them under [/modules] and restart)",
            annotate("check_cpu", {"CheckSystem", "CheckSystemUnix"}));
}

TEST(command_hints, message_keeps_the_prefix) {
  // Scripts match on "Unknown command(s): <name>"; the hint only follows it.
  const std::string msg = unknown_commands_message("Unknown command(s): ", {"check_ping"}, fake_providers);
  EXPECT_EQ(0u, msg.find("Unknown command(s): check_ping"));
  EXPECT_EQ("Unknown command(s): check_ping (provided by module CheckNet, which is not loaded: add \"CheckNet = enabled\" under [/modules] and restart)", msg);
}

TEST(command_hints, message_lists_every_command) {
  EXPECT_EQ(
      "Unknown command(s): check_ping (provided by module CheckNet, which is not loaded: add \"CheckNet = enabled\" under [/modules] and restart), "
      "my_script",
      unknown_commands_message("Unknown command(s): ", {"check_ping", "my_script"}, fake_providers));
}

TEST(command_hints, generated_map_is_looked_up_case_insensitively) {
  // Whatever this build's modules are, every entry of the generated map is
  // found again, in any case, the way the command registry keys commands.
  const auto *first = nsclient::core::generated::command_module_map;
  if (first->command == nullptr) GTEST_SKIP() << "no modules in this build";
  for (const auto *e = first; e->command != nullptr; ++e) {
    const std::string cmd = e->command;
    EXPECT_EQ(cmd, boost::algorithm::to_lower_copy(cmd)) << "map keys are lower case";
    const std::vector<std::string> found = providers_of(boost::algorithm::to_upper_copy(cmd));
    EXPECT_NE(found.end(), std::find(found.begin(), found.end(), std::string(e->module))) << cmd;
  }
  EXPECT_TRUE(providers_of("no_such_command_anywhere").empty());
}

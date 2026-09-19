// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// How the module reads the sub command out of a command line request. Plain
// C++: no runtime, no managed code, no PowerShell. Locating and starting the
// .NET runtime is tested in libs/dotnet_host, and running scripts in
// tests/powershell-script.test.ts.

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>

#include "PowerShellScript.h"

// Unit-test binaries have no generated module glue, so define the plugin
// singleton the log macros in PowerShellScript.cpp refer to.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

TEST(powershell_cli, reads_the_sub_command_from_the_command) {
  // `nscp client --module PowerShellScript --exec list` and the REST execute
  // path both put the sub command in the command field.
  EXPECT_EQ("list", PowerShellScript::cli_command("list", {}, false));
  EXPECT_EQ("list", PowerShellScript::cli_command("list", {"--all"}, true));
  EXPECT_EQ("execute", PowerShellScript::cli_command("execute", {"--script", "hello.ps1"}, true));
}

TEST(powershell_cli, reads_the_sub_command_from_the_first_argument) {
  // `nscp powershell list` targets the module with an empty command and the
  // sub command as the first argument.
  EXPECT_EQ("list", PowerShellScript::cli_command("", {"list"}, true));
  EXPECT_EQ("execute", PowerShellScript::cli_command("", {"execute", "--script", "hello.ps1"}, true));
  // ext-scr is the shared spelling the script modules answer to.
  EXPECT_EQ("list", PowerShellScript::cli_command("ext-scr", {"list"}, true));
  EXPECT_EQ("list", PowerShellScript::cli_command("ext-scr", {"list"}, false));
}

TEST(powershell_cli, bare_module_target_asks_for_help) {
  EXPECT_EQ("help", PowerShellScript::cli_command("", {}, true));
  // Not targeted at this module and nothing to go on: not ours to answer.
  EXPECT_EQ("", PowerShellScript::cli_command("", {}, false));
  EXPECT_EQ("", PowerShellScript::cli_command("", {"list"}, false));
  // ext-scr without arguments carries no sub command either.
  EXPECT_EQ("ext-scr", PowerShellScript::cli_command("ext-scr", {}, true));
}

TEST(powershell_perfdata, splits_the_nagios_message_and_perfdata_shape) {
  PB::Commands::QueryResponseMessage::Response response;
  response.add_lines()->set_message("OK: all good|'load'=0.5;1;2;0;4");
  PowerShellScript::split_perfdata(&response);
  ASSERT_EQ(1, response.lines_size());
  EXPECT_EQ("OK: all good", response.lines(0).message());
  ASSERT_EQ(1, response.lines(0).perf_size());
  EXPECT_EQ("load", response.lines(0).perf(0).alias());
  EXPECT_DOUBLE_EQ(0.5, response.lines(0).perf(0).float_value().value());
  EXPECT_DOUBLE_EQ(1.0, response.lines(0).perf(0).float_value().warning().value());
  EXPECT_DOUBLE_EQ(2.0, response.lines(0).perf(0).float_value().critical().value());
}

TEST(powershell_perfdata, leaves_a_line_without_perfdata_alone) {
  PB::Commands::QueryResponseMessage::Response response;
  response.add_lines()->set_message("OK: nothing to report");
  PowerShellScript::split_perfdata(&response);
  ASSERT_EQ(1, response.lines_size());
  EXPECT_EQ("OK: nothing to report", response.lines(0).message());
  EXPECT_EQ(0, response.lines(0).perf_size());
}

TEST(powershell_perfdata, leaves_a_line_that_already_carries_perfdata_alone) {
  // A script that built the performance data itself keeps it, pipe or no pipe.
  PB::Commands::QueryResponseMessage::Response response;
  PB::Commands::QueryResponseMessage::Response::Line *line = response.add_lines();
  line->set_message("OK: 3 | 4 disks checked");
  PB::Common::PerformanceData *perf = line->add_perf();
  perf->set_alias("disks");
  perf->mutable_float_value()->set_value(4);
  PowerShellScript::split_perfdata(&response);
  EXPECT_EQ("OK: 3 | 4 disks checked", response.lines(0).message());
  ASSERT_EQ(1, response.lines(0).perf_size());
  EXPECT_EQ("disks", response.lines(0).perf(0).alias());
}

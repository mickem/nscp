// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the CheckExternalScripts module class: the settings it
// registers, the default wrappings and aliases it seeds, what it registers
// with the core, and query_fallback - the dispatch every script and alias
// goes through, with its argument policy (allow arguments, nasty characters,
// NUL bytes, the Linux refusal of run-as keys).
//
// Aliases run their target command through the core, which the stub core
// answers, so they need nothing real. Scripts start a process: the few tests
// that let one run use /bin/sh with a fixed command line (no network, no host
// state) and are compiled on POSIX only. Every refusal is decided before a
// process would start.

#include "CheckExternalScripts.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <boost/filesystem.hpp>
#include <fstream>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/test_helpers.hpp>
#include <string>
#include <utility>
#include <vector>

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

const char *const kRoot = "/settings/external scripts";

class ExternalScriptsModule : public ::testing::Test {
 protected:
  nscapi::test_helpers::stub_core &core() { return nscapi::test_helpers::stub_core::instance(); }

  void SetUp() override {
    core().reset();
    module_.set_id(42);
  }
  void TearDown() override {
    module_.unloadModule();
    core().reset();
  }

  bool load(const NSCAPI::moduleLoadMode mode = NSCAPI::dontStart) { return module_.loadModuleEx("", mode); }

  void scripts(const std::vector<std::pair<std::string, std::string> > &entries) { core().set_keys("external scripts/scripts", entries); }
  void aliases(const std::vector<std::pair<std::string, std::string> > &entries) { core().set_keys("external scripts/alias", entries); }

  PB::Commands::QueryResponseMessage::Response query(const std::string &command, const std::vector<std::string> &args = {}) {
    PB::Commands::QueryRequestMessage request_message;
    PB::Commands::QueryRequestMessage::Request request;
    request.set_command(command);
    for (const std::string &a : args) request.add_arguments(a);
    PB::Commands::QueryResponseMessage::Response response;
    module_.query_fallback(request, &response, request_message);
    return response;
  }

  PB::Commands::ExecuteResponseMessage::Response cli(const std::vector<std::string> &args, bool *handled = nullptr) {
    PB::Commands::ExecuteRequestMessage request_message;
    PB::Commands::ExecuteRequestMessage::Request request;
    request.set_command("ext-scr");
    for (const std::string &a : args) request.add_arguments(a);
    PB::Commands::ExecuteResponseMessage::Response response;
    const bool ret = module_.commandLineExec(NSCAPI::target_module, request, &response, request_message);
    if (handled != nullptr) *handled = ret;
    return response;
  }

  static std::string message_of(const PB::Commands::QueryResponseMessage::Response &r) { return r.lines_size() > 0 ? r.lines(0).message() : std::string(); }

  CheckExternalScripts module_;
};

}  // namespace

// ============================================================================
// The settings contract
// ============================================================================

TEST_F(ExternalScriptsModule, LoadRegistersItsKeysWithTheDocumentedDefaults) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for(kRoot, "timeout"), "60");
  EXPECT_EQ(core().default_for(kRoot, "kill tree"), "false");
  EXPECT_EQ(core().default_for(kRoot, "allow arguments"), "false");
  EXPECT_EQ(core().default_for(kRoot, "allow nasty characters"), "false");
  EXPECT_EQ(core().default_for(kRoot, "script path"), "");
}

// The script root is shared with the other script modules and inherited from
// /settings/default, so one fleet setting moves all of them.
TEST_F(ExternalScriptsModule, TheScriptRootIsSharedThroughTheDefaultSection) {
  ASSERT_TRUE(load());

  EXPECT_TRUE(core().has_key("/settings/default", "script root"));
  EXPECT_EQ(core().default_for(kRoot, "script root"), "");
}

// The default wrappings call the interpreter directly rather than through
// cmd.exe, which is what keeps $ARGn$ out of cmd's parser.
TEST_F(ExternalScriptsModule, DefaultWrappingsArePublished) {
  ASSERT_TRUE(load());

  const std::string wrappings = std::string(kRoot) + "/wrappings";
  EXPECT_EQ(core().default_for(wrappings, "ps1"), "powershell.exe -NoProfile -ExecutionPolicy Bypass -NonInteractive -File \"scripts\\%SCRIPT%\" $ARGS\"$");
  EXPECT_EQ(core().default_for(wrappings, "vbs"), "cscript.exe //T:30 //NoLogo \"scripts\\lib\\wrapper.vbs\" %SCRIPT% $ARGS\"$");
  EXPECT_EQ(core().default_for(wrappings, "bat"), "\"scripts\\%SCRIPT%\" $ARGS\"$");
  EXPECT_EQ(core().default_for(wrappings, "ps1").find("cmd"), std::string::npos);
}

TEST_F(ExternalScriptsModule, AConfiguredWrappingIsNotOverwritten) {
  core().set_keys("external scripts/wrappings", {{"ps1", "pwsh -File scripts/%SCRIPT% $ARGS$"}});

  ASSERT_TRUE(load());
  EXPECT_FALSE(core().has_key(std::string(kRoot) + "/wrappings", "ps1")) << "the default is only registered when none is configured";
  EXPECT_TRUE(core().has_key(std::string(kRoot) + "/wrappings", "bat"));
}

// ============================================================================
// What is registered with the core
// ============================================================================

TEST_F(ExternalScriptsModule, WithoutAliasesTheDefaultSetIsRegistered) {
  ASSERT_TRUE(load());

  for (const char *name : {"alias_cpu", "alias_cpu_ex", "alias_mem", "alias_up", "alias_disk", "alias_service", "alias_process", "alias_file_age"}) {
    EXPECT_TRUE(core().has_command(name)) << name;
  }
}

TEST_F(ExternalScriptsModule, ConfiguredAliasesReplaceTheDefaultSet) {
  aliases({{"my_cpu", "check_cpu warn=load>80"}});

  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_command("my_cpu"));
  EXPECT_FALSE(core().has_command("alias_cpu"));
  EXPECT_FALSE(core().has_command("default")) << "the template is not a command";
}

TEST_F(ExternalScriptsModule, ConfiguredScriptsAreRegistered) {
  scripts({{"check_ok", "/bin/true"}, {"check_other", "/bin/false"}});

  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_command("check_ok"));
  EXPECT_TRUE(core().has_command("check_other"));
}

TEST_F(ExternalScriptsModule, WrappedScriptsUseTheWrappingForTheirExtension) {
  core().set_keys("external scripts/wrappings", {{"sh", "/bin/sh scripts/%SCRIPT% %ARGS%"}});
  core().set_keys("external scripts/wrapped scripts", {{"check_wrapped", "check_it.sh -a 1"}, {"check_unknown", "check_it.xyz"}});

  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_command("check_wrapped"));
  EXPECT_TRUE(core().has_logged("Failed to find wrapping for type: xyz"));
}

// Every file in `script path` becomes a command named after the file.
TEST_F(ExternalScriptsModule, ScriptPathRegistersEveryFileInTheFolder) {
  const boost::filesystem::path dir = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("extscr-test-%%%%-%%%%");
  boost::filesystem::create_directories(dir / "sub");
  std::ofstream((dir / "check_a.sh").string().c_str()) << "#!/bin/sh\n";
  std::ofstream((dir / "check_b.bat").string().c_str()) << "@echo off\n";
  core().set_setting(kRoot, "script path", dir.string());

  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_command("check_a.sh"));
  EXPECT_TRUE(core().has_command("check_b.bat"));
  EXPECT_FALSE(core().has_command("sub")) << "folders are not commands";

  boost::system::error_code ec;
  boost::filesystem::remove_all(dir, ec);
}

// A relative `script path` is rooted at ${base-path}, and its last component
// may be a glob.
TEST_F(ExternalScriptsModule, ScriptPathWithAGlobOnlyRegistersMatchingFiles) {
  const boost::filesystem::path dir = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("extscr-test-%%%%-%%%%");
  boost::filesystem::create_directories(dir / "scripts");
  std::ofstream((dir / "scripts" / "check_a.sh").string().c_str()) << "#!/bin/sh\n";
  std::ofstream((dir / "scripts" / "notes.txt").string().c_str()) << "x\n";
  core().set_path("${base-path}", dir.string());
  core().set_setting(kRoot, "script path", "scripts/*.sh");

  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_command("check_a.sh"));
  EXPECT_FALSE(core().has_command("notes.txt"));

  boost::system::error_code ec;
  boost::filesystem::remove_all(dir, ec);
}

TEST_F(ExternalScriptsModule, AMissingScriptPathIsReported) {
  core().set_path("${base-path}", "/nonexistent-nscp-test-dir");
  core().set_setting(kRoot, "script path", "sub/*.sh");

  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_logged("Path was not found: /nonexistent-nscp-test-dir/sub")) << (core().logged().empty() ? "" : core().logged().back().message);
}

TEST_F(ExternalScriptsModule, ReloadIsSafe) {
  aliases({{"my_cpu", "check_cpu"}});
  scripts({{"check_ok", "/bin/true"}});

  ASSERT_TRUE(load());
  ASSERT_TRUE(load(NSCAPI::reloadStart));
  EXPECT_TRUE(core().has_command("my_cpu"));
  EXPECT_TRUE(core().has_command("check_ok"));
}

// ============================================================================
// query_fallback: aliases
// ============================================================================

TEST_F(ExternalScriptsModule, AnUnknownCommandIsReported) {
  ASSERT_TRUE(load());

  const PB::Commands::QueryResponseMessage::Response r = query("check_nothing");
  EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_EQ(message_of(r), "No command or alias found matching: check_nothing");
}

TEST_F(ExternalScriptsModule, AnAliasRunsItsCommandWithTheArgumentsSubstituted) {
  aliases({{"my_cpu", "check_cpu \"warn=load > $ARG1$\" \"crit=load > %ARG2%\" time=5m"}});
  core().set_query_result("check_cpu", PB::Common::ResultCode::WARNING, "CPU high");

  ASSERT_TRUE(load());
  const PB::Commands::QueryResponseMessage::Response r = query("my_cpu", {"80", "90"});

  EXPECT_EQ(r.result(), PB::Common::ResultCode::WARNING);
  EXPECT_EQ(message_of(r), "CPU high");
  ASSERT_EQ(core().queries().size(), 1u);
  const PB::Commands::QueryRequestMessage::Request &q = core().queries()[0].payload(0);
  EXPECT_EQ(q.command(), "check_cpu");
  ASSERT_EQ(q.arguments_size(), 3);
  EXPECT_EQ(q.arguments(0), "warn=load > 80");
  EXPECT_EQ(q.arguments(1), "crit=load > 90");
  EXPECT_EQ(q.arguments(2), "time=5m");
}

// An alias dispatches to another internal check, so `allow arguments` (which
// governs processes) does not apply to it.
TEST_F(ExternalScriptsModule, AliasArgumentsAreSubstitutedEvenWithArgumentsDisallowed) {
  aliases({{"my_proc", "check_process process=$ARG1$"}});
  core().set_query_result("check_process", PB::Common::ResultCode::OK, "running");

  ASSERT_TRUE(load());
  EXPECT_EQ(message_of(query("my_proc", {"nscp"})), "running");
  EXPECT_EQ(core().queries()[0].payload(0).arguments(0), "process=nscp");
}

TEST_F(ExternalScriptsModule, AnAliasWhoseCommandFailsIsReported) {
  aliases({{"my_cpu", "check_cpu"}});

  ASSERT_TRUE(load());
  const PB::Commands::QueryResponseMessage::Response r = query("my_cpu");
  EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_EQ(message_of(r), "Failed to execute: my_cpu");
}

TEST_F(ExternalScriptsModule, AnAliasWhoseCommandAnswersTwiceIsRejected) {
  aliases({{"my_cpu", "check_cpu"}});
  PB::Commands::QueryResponseMessage two;
  two.add_payload()->set_command("check_cpu");
  two.add_payload()->set_command("check_cpu");
  core().set_query_response("check_cpu", two);

  ASSERT_TRUE(load());
  EXPECT_EQ(message_of(query("my_cpu")), "Invalid response from command: my_cpu");
}

// help-pb lists each $ARGn$ placeholder once, in order, with the argument it
// sits in.
TEST_F(ExternalScriptsModule, HelpPbListsThePlaceholders) {
  aliases({{"my_cpu", "check_cpu \"warn=load > $ARG1$\" \"crit=load > %ARG2%\""}});

  ASSERT_TRUE(load());
  const PB::Commands::QueryResponseMessage::Response r = query("my_cpu", {"help-pb"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK);
  EXPECT_EQ(message_of(r), "$ARG1$,false,,warn=load > $ARG1$\n%ARG2%,false,,crit=load > %ARG2%\n");
  EXPECT_TRUE(core().queries().empty());
}

// ============================================================================
// query_fallback: the argument policy for scripts
// ============================================================================

TEST_F(ExternalScriptsModule, ArgumentsToAScriptAreRefusedByDefault) {
  scripts({{"check_it", "/bin/echo $ARG1$"}});

  ASSERT_TRUE(load());
  const PB::Commands::QueryResponseMessage::Response r = query("check_it", {"hello"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_EQ(message_of(r), "Arguments not allowed see nsclient.log for details");
}

TEST_F(ExternalScriptsModule, NastyCharactersAreRefused) {
  core().set_setting(kRoot, "allow arguments", "true");
  scripts({{"check_it", "/bin/echo $ARG1$"}});

  ASSERT_TRUE(load());
  for (const char *nasty : {"a|b", "a`b`", "a>b", "a&b", "a'b", "a{b}"}) {
    const PB::Commands::QueryResponseMessage::Response r = query("check_it", {nasty});
    EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN) << nasty;
    EXPECT_EQ(message_of(r), "Request contained illegal characters set /settings/external scripts/allow nasty characters=true!") << nasty;
  }
}

// A NUL would truncate the command line on Windows; it is refused whatever
// `allow nasty characters` says.
TEST_F(ExternalScriptsModule, ANulByteIsRefusedEvenWhenNastyCharactersAreAllowed) {
  core().set_setting(kRoot, "allow arguments", "true");
  core().set_setting(kRoot, "allow nasty characters", "true");
  scripts({{"check_it", "/bin/echo $ARG1$ --read-only"}});

  ASSERT_TRUE(load());
  const PB::Commands::QueryResponseMessage::Response r = query("check_it", {std::string("x\0y", 3)});
  EXPECT_EQ(message_of(r), "Request contained a NUL byte in an argument, which would truncate the command line.");
}

#ifndef WIN32
// The run-as keys are Windows only; on Linux a script that sets them is
// refused rather than silently run as the service account.
TEST_F(ExternalScriptsModule, RunAsKeysAreRefusedOnLinux) {
  core().set_sections("external scripts/scripts", {"check_secure"});
  core().set_keys("external scripts/scripts/check_secure", {{"command", "/bin/true"}, {"user", "nobody"}});

  ASSERT_TRUE(load());
  ASSERT_TRUE(core().has_command("check_secure"));
  const PB::Commands::QueryResponseMessage::Response r = query("check_secure");
  EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_EQ(message_of(r).find("Refusing to run check_secure: the user, domain and password settings are only supported on Windows"), 0u) << message_of(r);
}

// ============================================================================
// query_fallback: running a script (POSIX, /bin/sh only)
// ============================================================================

TEST_F(ExternalScriptsModule, AScriptsExitCodeAndOutputBecomeTheResult) {
  scripts({{"check_warn", "/bin/sh -c \"echo 'disk is filling|used=80%;70;90'; exit 1\""}});

  ASSERT_TRUE(load());
  const PB::Commands::QueryResponseMessage::Response r = query("check_warn");
  EXPECT_EQ(r.result(), PB::Common::ResultCode::WARNING);
  ASSERT_EQ(r.lines_size(), 1);
  EXPECT_EQ(r.lines(0).message(), "disk is filling");
  ASSERT_EQ(r.lines(0).perf_size(), 1);
  EXPECT_EQ(r.lines(0).perf(0).alias(), "used");
}

TEST_F(ExternalScriptsModule, IgnorePerfdataKeepsThePipeInTheMessage) {
  core().set_sections("external scripts/scripts", {"check_raw"});
  core().set_keys("external scripts/scripts/check_raw", {{"command", "/bin/sh -c \"echo 'a|b=1'\""}, {"ignore perfdata", "true"}});

  ASSERT_TRUE(load());
  const PB::Commands::QueryResponseMessage::Response r = query("check_raw");
  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK);
  EXPECT_EQ(message_of(r), "a|b=1");
  EXPECT_EQ(r.lines(0).perf_size(), 0);
}

TEST_F(ExternalScriptsModule, AScriptWithNoOutputSaysSo) {
  scripts({{"check_silent", "/bin/true"}});

  ASSERT_TRUE(load());
  const PB::Commands::QueryResponseMessage::Response r = query("check_silent");
  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK);
  EXPECT_EQ(message_of(r), "No output available from command (check_silent).");
}

TEST_F(ExternalScriptsModule, AnExitCodeOutsideNagiosRangeIsReported) {
  scripts({{"check_odd", "/bin/sh -c \"exit 7\""}});

  ASSERT_TRUE(load());
  const PB::Commands::QueryResponseMessage::Response r = query("check_odd");
  EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_EQ(message_of(r), "The command (check_odd) returned an invalid return code: 7");
}

// With arguments allowed each $ARGn$ becomes exactly one argv element: a
// value with spaces is not split, and is not handed to a shell.
TEST_F(ExternalScriptsModule, AllowedArgumentsAreSubstitutedAsSingleArgvElements) {
  core().set_setting(kRoot, "allow arguments", "true");
  scripts({{"check_args", "/bin/sh -c \"echo \\\"[$0][$1]\\\"\" $ARG1$ $ARG2$"}});

  ASSERT_TRUE(load());
  const PB::Commands::QueryResponseMessage::Response r = query("check_args", {"one two", "three"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK) << message_of(r);
  EXPECT_EQ(message_of(r), "[one two][three]");
}
#endif

// ============================================================================
// `nscp ext-scr`
// ============================================================================

TEST_F(ExternalScriptsModule, CliWithoutASubcommandPrintsUsage) {
  ASSERT_TRUE(load());

  PB::Commands::ExecuteRequestMessage request_message;
  PB::Commands::ExecuteRequestMessage::Request request;
  PB::Commands::ExecuteResponseMessage::Response response;
  EXPECT_TRUE(module_.commandLineExec(NSCAPI::target_module, request, &response, request_message));
  EXPECT_EQ(response.message(), "Usage: nscp ext-scr [add|list|show|install|delete] --help");
}

TEST_F(ExternalScriptsModule, CliBeforeLoadHasNoProvider) {
  bool handled = false;
  const PB::Commands::ExecuteResponseMessage::Response r = cli({"list"}, &handled);
  EXPECT_TRUE(handled);
  EXPECT_EQ(r.message(), "Failed to create provider");
}

TEST_F(ExternalScriptsModule, CliListQueryShowsTheConfiguredCommands) {
  scripts({{"check_ok", "/bin/true"}});

  ASSERT_TRUE(load());
  bool handled = false;
  const PB::Commands::ExecuteResponseMessage::Response r = cli({"list", "--query"}, &handled);
  EXPECT_TRUE(handled);
  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK) << r.message();
  EXPECT_NE(r.message().find("check_ok\n"), std::string::npos) << r.message();
}

// A scripts folder that is not there is not an empty one.
TEST_F(ExternalScriptsModule, CliListOfAMissingScriptsFolderSaysSo) {
  core().set_path("${scripts}", "/nonexistent-nscp-test-dir/scripts");

  ASSERT_TRUE(load());
  const PB::Commands::ExecuteResponseMessage::Response r = cli({"list"});
  EXPECT_NE(r.result(), PB::Common::ResultCode::OK);
  EXPECT_EQ(r.message().find("Failed to list /nonexistent-nscp-test-dir/scripts: "), 0u) << r.message();
}

// The configured script root is the folder `list` walks.
TEST_F(ExternalScriptsModule, CliListShowsTheScriptRoot) {
  const boost::filesystem::path dir = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("extscr-test-%%%%-%%%%");
  boost::filesystem::create_directories(dir / "scripts");
  std::ofstream((dir / "scripts" / "check_listed.sh").string().c_str()) << "#!/bin/sh\n";
  core().set_setting(kRoot, "script root", (dir / "scripts").string());

  ASSERT_TRUE(load());
  const PB::Commands::ExecuteResponseMessage::Response r = cli({"list"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK) << r.message();
  EXPECT_NE(r.message().find("check_listed.sh"), std::string::npos) << r.message();

  boost::system::error_code ec;
  boost::filesystem::remove_all(dir, ec);
}

TEST_F(ExternalScriptsModule, QueryBeforeLoadHasNoProvider) {
  const PB::Commands::QueryResponseMessage::Response r = query("check_ok");
  EXPECT_EQ(message_of(r), "No command or alias found matching: check_ok");
}

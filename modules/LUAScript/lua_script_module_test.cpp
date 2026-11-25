// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the LUAScript module class: the settings it registers, how
// configured scripts are found and loaded, what they register with the core,
// and how queries, command-line calls and submissions reach the functions a
// script registered.
//
// Loading a script runs its top-level code in an in-process Lua state - that
// is how a script registers its functions, whatever the load mode - so the
// tests that load one write a small script of their own to a temporary
// script root. Nothing starts a thread or touches the network, and no test
// depends on the scripts shipped in the repository. on_start only runs from
// startModule, which only the lifecycle test calls.

#include "LUAScript.h"

#include <gtest/gtest.h>

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

const char *const kRoot = "/settings/lua";

// Registers one function of each kind the module dispatches to.
const char *const kScript = R"(
function test_query(command, args)
  return 'warning', command .. '::' .. table.concat(args, ','), "'value'=5"
end
function test_exec(command, args)
  return 'ok', 'exec ' .. table.concat(args, ',')
end
function test_submit(channel, command, result, msgs)
  return result == 'ok', 'got ' .. command .. ' ' .. result
end
local reg = Registry()
reg:simple_function('lua_test_query', test_query, 'A query from the test script')
reg:simple_cmdline('lua_test_exec', test_exec, 'A command from the test script')
reg:simple_subscription('lua_test_channel', test_submit, 'A channel from the test script')
)";

typedef PB::Commands::QueryResponseMessage::Response query_response;

std::string message_of(const query_response &r) { return r.lines_size() > 0 ? r.lines(0).message() : std::string(); }

class LuaScriptModule : public ::testing::Test {
 protected:
  nscapi::test_helpers::stub_core &core() { return nscapi::test_helpers::stub_core::instance(); }

  void SetUp() override {
    core().reset();
    module_.set_id(42);
    root_ = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("lua-test-%%%%-%%%%");
    boost::filesystem::create_directories(root_ / "lua");
    core().set_setting(kRoot, "script root", root_.string());
  }
  void TearDown() override {
    module_.unloadModule();
    core().reset();
    boost::system::error_code ec;
    boost::filesystem::remove_all(root_, ec);
  }

  bool load(const NSCAPI::moduleLoadMode mode = NSCAPI::dontStart) { return module_.loadModuleEx("lua", mode); }

  void write_script(const std::string &name, const std::string &body) { std::ofstream((root_ / "lua" / name).string().c_str()) << body; }
  void scripts(const std::vector<std::pair<std::string, std::string> > &entries) { core().set_keys("lua/scripts", entries); }

  query_response query(const std::string &command, const std::vector<std::string> &args = {}) {
    PB::Commands::QueryRequestMessage request_message;
    PB::Commands::QueryRequestMessage::Request request;
    request.set_command(command);
    for (const std::string &a : args) request.add_arguments(a);
    query_response response;
    module_.query_fallback(request, &response, request_message);
    return response;
  }

  PB::Commands::ExecuteResponseMessage::Response exec(const std::string &command, const std::vector<std::string> &args, bool *handled = nullptr,
                                                      const int target = NSCAPI::target_module) {
    PB::Commands::ExecuteRequestMessage request_message;
    PB::Commands::ExecuteRequestMessage::Request request;
    request.set_command(command);
    for (const std::string &a : args) request.add_arguments(a);
    PB::Commands::ExecuteResponseMessage::Response response;
    const bool ret = module_.commandLineExec(target, request, &response, request_message);
    if (handled != nullptr) *handled = ret;
    return response;
  }

  boost::filesystem::path root_;
  nscapi::test_helpers::stdout_capture out_;
  LUAScript module_;
};

}  // namespace

// ============================================================================
// The settings contract
// ============================================================================

TEST_F(LuaScriptModule, LoadRegistersTheScriptsSectionAndTheSharedScriptRoot) {
  ASSERT_TRUE(load());

  const std::vector<std::string> &paths = core().registered_paths();
  EXPECT_NE(std::find(paths.begin(), paths.end(), std::string(kRoot) + "/scripts"), paths.end());
  EXPECT_TRUE(core().has_key("/settings/default", "script root"));
  EXPECT_EQ(core().default_for(kRoot, "script root"), "");
}

TEST_F(LuaScriptModule, WithoutScriptsNothingIsRegistered) {
  ASSERT_TRUE(load());

  EXPECT_TRUE(core().registered_commands().empty());
  EXPECT_TRUE(core().registered_channels().empty());
}

// ============================================================================
// Loading scripts
// ============================================================================

TEST_F(LuaScriptModule, AScriptRegistersItsFunctions) {
  write_script("test_mod.lua", kScript);
  scripts({{"test_mod", "test_mod"}});

  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_command("lua_test_query"));
  EXPECT_TRUE(core().has_channel("lua_test_channel"));
}

// The .lua extension may be left off, and a bare key names the script.
TEST_F(LuaScriptModule, AScriptIsFoundByNameUnderTheLuaFolder) {
  write_script("test_mod.lua", kScript);
  scripts({{"test_mod", ""}});

  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_command("lua_test_query"));
}

TEST_F(LuaScriptModule, AMissingScriptIsReportedAndTheOthersLoad) {
  write_script("test_mod.lua", kScript);
  scripts({{"gone", "no_such_script"}, {"test_mod", "test_mod"}});

  ASSERT_TRUE(load());
  EXPECT_TRUE(out_.contains("Failed to find script: no_such_script"));
  EXPECT_TRUE(core().has_command("lua_test_query"));
}

// One script that does not parse must not take the others down with it.
TEST_F(LuaScriptModule, AScriptThatDoesNotParseIsReportedAndTheOthersLoad) {
  write_script("broken.lua", "this is not lua (\n");
  write_script("test_mod.lua", kScript);
  scripts({{"broken", "broken"}, {"test_mod", "test_mod"}});

  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_command("lua_test_query"));
  EXPECT_TRUE(core().has_logged("broken")) << "the parse error names the script";
}

// A reload unloads the previous generation and retracts what the new one no
// longer registers.
TEST_F(LuaScriptModule, ReloadRetractsTheCommandsOfARemovedScript) {
  write_script("test_mod.lua", kScript);
  scripts({{"test_mod", "test_mod"}});
  ASSERT_TRUE(load());
  ASSERT_TRUE(core().has_command("lua_test_query"));

  scripts({});
  ASSERT_TRUE(load(NSCAPI::reloadStart));
  EXPECT_FALSE(core().has_command("lua_test_query"));
  EXPECT_EQ(message_of(query("lua_test_query")), "Failed to find command: lua_test_query");
}

TEST_F(LuaScriptModule, StartAndUnloadAreSafe) {
  write_script("test_mod.lua", kScript);
  scripts({{"test_mod", "test_mod"}});
  ASSERT_TRUE(load());

  EXPECT_TRUE(module_.startModule());
  EXPECT_TRUE(module_.unloadModule());
  EXPECT_EQ(message_of(query("lua_test_query")), "Module is not loaded");
  EXPECT_TRUE(module_.unloadModule());
}

// ============================================================================
// Dispatch into a script
// ============================================================================

TEST_F(LuaScriptModule, AQueryRunsTheScriptFunction) {
  write_script("test_mod.lua", kScript);
  scripts({{"test_mod", "test_mod"}});
  ASSERT_TRUE(load());

  const query_response r = query("lua_test_query", {"a", "b"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::WARNING);
  EXPECT_EQ(message_of(r), "lua_test_query::a,b");
  ASSERT_EQ(r.lines_size(), 1);
  ASSERT_EQ(r.lines(0).perf_size(), 1);
  EXPECT_EQ(r.lines(0).perf(0).alias(), "value");
}

// The core asks for a command's parameters with a lone help-pb; a simple
// function cannot tell that from a real call, so it is answered without
// running the script.
TEST_F(LuaScriptModule, HelpPbIsAnsweredWithoutRunningTheFunction) {
  write_script("test_mod.lua", kScript);
  scripts({{"test_mod", "test_mod"}});
  ASSERT_TRUE(load());

  const query_response r = query("lua_test_query", {"help-pb"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK);
  EXPECT_EQ(r.lines_size(), 0) << message_of(r);
  EXPECT_EQ(r.data(), "");
}

TEST_F(LuaScriptModule, AnUnknownQueryIsReported) {
  ASSERT_TRUE(load());

  const query_response r = query("no_such_function");
  EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_EQ(message_of(r), "Failed to find command: no_such_function");
}

TEST_F(LuaScriptModule, ASubmissionReachesTheSubscribedFunction) {
  write_script("test_mod.lua", kScript);
  scripts({{"test_mod", "test_mod"}});
  ASSERT_TRUE(load());

  PB::Commands::QueryResponseMessage::Response payload;
  payload.set_command("check_cpu");
  payload.set_result(PB::Common::ResultCode::OK);
  payload.add_lines()->set_message("fine");
  PB::Commands::SubmitRequestMessage request_message;
  PB::Commands::SubmitResponseMessage::Response response;
  module_.handleNotification("lua_test_channel", payload, &response, request_message);

  EXPECT_EQ(response.result().code(), PB::Common::Result_StatusCodeType_STATUS_OK);
  EXPECT_EQ(response.result().message(), "got check_cpu ok");
}

// What the function returns first is whether the submission was accepted.
TEST_F(LuaScriptModule, ASubscribedFunctionCanRejectASubmission) {
  write_script("test_mod.lua", kScript);
  scripts({{"test_mod", "test_mod"}});
  ASSERT_TRUE(load());

  PB::Commands::QueryResponseMessage::Response payload;
  payload.set_command("check_cpu");
  payload.set_result(PB::Common::ResultCode::CRITICAL);
  PB::Commands::SubmitRequestMessage request_message;
  PB::Commands::SubmitResponseMessage::Response response;
  module_.handleNotification("lua_test_channel", payload, &response, request_message);

  EXPECT_NE(response.result().code(), PB::Common::Result_StatusCodeType_STATUS_OK);
  EXPECT_EQ(response.result().message(), "got check_cpu critical");
}

TEST_F(LuaScriptModule, ASubmissionOnAnUnknownChannelIsIgnored) {
  ASSERT_TRUE(load());

  PB::Commands::QueryResponseMessage::Response payload;
  PB::Commands::SubmitRequestMessage request_message;
  PB::Commands::SubmitResponseMessage::Response response;
  module_.handleNotification("nobody", payload, &response, request_message);
  EXPECT_FALSE(response.has_result());
}

// ============================================================================
// `nscp lua ...` and script command lines
// ============================================================================

TEST_F(LuaScriptModule, WithoutASubcommandUsageIsPrinted) {
  ASSERT_TRUE(load());

  bool handled = false;
  const PB::Commands::ExecuteResponseMessage::Response r = exec("", {}, &handled);
  EXPECT_TRUE(handled);
  EXPECT_EQ(r.message(), "Usage: nscp lua [add|execute|list|show|install|delete] --help");
}

TEST_F(LuaScriptModule, AScriptCommandLineIsRun) {
  write_script("test_mod.lua", kScript);
  scripts({{"test_mod", "test_mod"}});
  ASSERT_TRUE(load());

  bool handled = false;
  const PB::Commands::ExecuteResponseMessage::Response r = exec("lua_test_exec", {"x", "y"}, &handled);
  EXPECT_TRUE(handled);
  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK);
  EXPECT_EQ(r.message(), "exec x,y");
}

// Not a verb of the module and not a script's command: left to others.
TEST_F(LuaScriptModule, AnUnknownCommandLineIsNotClaimed) {
  ASSERT_TRUE(load());

  bool handled = true;
  exec("no_such_command", {}, &handled, NSCAPI::target_any);
  EXPECT_FALSE(handled);
}

TEST_F(LuaScriptModule, ExecuteReportsAMissingScript) {
  ASSERT_TRUE(load());

  const PB::Commands::ExecuteResponseMessage::Response r = exec("execute", {"--script", "no_such_script"});
  EXPECT_NE(r.result(), PB::Common::ResultCode::OK);
  EXPECT_EQ(r.message(), "Script not found: no_such_script");
}

TEST_F(LuaScriptModule, ExecuteRunsAScriptsMain) {
  write_script("run_me.lua", "function main(args)\n  return 'ok', 'ran with ' .. table.concat(args, ',')\nend\n");
  ASSERT_TRUE(load());

  const PB::Commands::ExecuteResponseMessage::Response r = exec("execute", {"--script", "run_me", "p1"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK) << r.message();
  EXPECT_EQ(r.message(), "ran with p1");
}

TEST_F(LuaScriptModule, ListShowsTheScriptsInTheScriptRoot) {
  write_script("test_mod.lua", kScript);
  ASSERT_TRUE(load());

  const PB::Commands::ExecuteResponseMessage::Response r = exec("list", {});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK) << r.message();
  EXPECT_NE(r.message().find("test_mod"), std::string::npos) << r.message();
}

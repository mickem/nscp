// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <memory>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <string>

#include "script_provider.hpp"

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
static nscapi::helper_singleton test_plugin_singleton;
nscapi::helper_singleton *nscapi::plugin_singleton = &test_plugin_singleton;

namespace {

// Minimal core stubs: every settings/registry query succeeds with an empty
// response, so settings keys resolve to their in-code defaults and command
// registration is a no-op. Log endpoints stay null (harmless no-ops).
NSCAPI::errorReturn stub_query(const char *, const unsigned int, char **response, unsigned int *response_len) {
  *response = nullptr;
  *response_len = 0;
  return NSCAPI::api_return_codes::isSuccess;
}

void stub_destroy_buffer(char **buffer) {
  delete[] *buffer;
  *buffer = nullptr;
}

nscapi::core_api::FUNPTR stub_loader(const char *name) {
  const std::string n(name);
  if (n == "NSAPISettingsQuery" || n == "NSAPIRegistryQuery") return reinterpret_cast<nscapi::core_api::FUNPTR>(&stub_query);
  if (n == "NSAPIDestroyBuffer") return reinterpret_cast<nscapi::core_api::FUNPTR>(&stub_destroy_buffer);
  return nullptr;
}

nscapi::core_wrapper *test_core() {
  static nscapi::core_wrapper *core = [] {
    auto *c = new nscapi::core_wrapper();
    c->load_endpoints(&stub_loader);
    return c;
  }();
  return core;
}

std::shared_ptr<script_provider> make_provider(const std::map<std::string, std::string> &wrappings = {}, const int id = 42) {
  return std::make_shared<script_provider>(id, test_core(), "/settings/external scripts/scripts", boost::filesystem::path("/opt/nscp"), wrappings);
}

}  // namespace

// ----- trivial accessors ----------------------------------------------------

TEST(ScriptProvider, ExposesIdCoreAndRoot) {
  auto provider = make_provider();
  EXPECT_EQ(provider->get_id(), 42u);
  EXPECT_EQ(provider->get_core(), test_core());
  EXPECT_EQ(provider->get_root(), boost::filesystem::path("/opt/nscp"));
  EXPECT_TRUE(provider->get_settings_proxy());
}

// ----- generate_wrapped_command ---------------------------------------------

TEST(ScriptProvider, WrapsScriptByExtension) {
  auto provider = make_provider({{"sh", "/bin/sh scripts/%SCRIPT% %ARGS%"}});
  EXPECT_EQ(provider->generate_wrapped_command("check_test.sh foo bar"), "/bin/sh scripts/check_test.sh foo bar");
}

TEST(ScriptProvider, WrapsScriptWithoutArguments) {
  auto provider = make_provider({{"bat", "scripts\\%SCRIPT% %ARGS%"}});
  EXPECT_EQ(provider->generate_wrapped_command("check_test.bat"), "scripts\\check_test.bat ");
}

TEST(ScriptProvider, WrapsExtensionlessCommandViaNoneType) {
  auto provider = make_provider({{"none", "run %SCRIPT% %ARGS%"}});
  EXPECT_EQ(provider->generate_wrapped_command("somecommand arg1"), "run somecommand arg1");
}

TEST(ScriptProvider, UnknownWrappingTypeYieldsEmptyCommand) {
  auto provider = make_provider({{"sh", "/bin/sh scripts/%SCRIPT% %ARGS%"}});
  EXPECT_EQ(provider->generate_wrapped_command("check_test.py foo"), "");
}

// ----- command add/find/list/remove -----------------------------------------

TEST(ScriptProvider, AddAndFindCommand) {
  auto provider = make_provider();
  provider->add_command("check_test", "scripts/test.sh arg1");
  commands::command_object_instance obj = provider->find_command("check_test");
  ASSERT_TRUE(obj);
  EXPECT_EQ(obj->get_alias(), "check_test");
  EXPECT_EQ(obj->command, "scripts/test.sh arg1");
}

TEST(ScriptProvider, FindUnknownCommandReturnsEmpty) {
  auto provider = make_provider();
  EXPECT_FALSE(provider->find_command("no_such_command"));
}

TEST(ScriptProvider, GetCommandsListsAddedAliases) {
  auto provider = make_provider();
  EXPECT_TRUE(provider->get_commands().empty());
  provider->add_command("check_one", "one.sh");
  provider->add_command("check_two", "two.sh");
  const std::list<std::string> aliases = provider->get_commands();
  EXPECT_EQ(aliases.size(), 2u);
  EXPECT_NE(std::find(aliases.begin(), aliases.end(), "check_one"), aliases.end());
  EXPECT_NE(std::find(aliases.begin(), aliases.end(), "check_two"), aliases.end());
}

TEST(ScriptProvider, RemoveCommandDropsIt) {
  auto provider = make_provider();
  provider->add_command("check_gone", "gone.sh");
  ASSERT_TRUE(provider->find_command("check_gone"));
  provider->remove_command("check_gone");
  EXPECT_FALSE(provider->find_command("check_gone"));
  EXPECT_TRUE(provider->get_commands().empty());
}

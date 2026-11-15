// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// nscapi::settings_proxy against a stub core: the request each call puts on
// the wire and how it reads the answer back. proxy_test.cpp only builds the
// protobuf messages by hand; this drives the proxy itself.

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/settings/proxy.hpp>
#include <nscapi/test_helpers.hpp>
#include <string>

nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

class SettingsProxy : public ::testing::Test {
 protected:
  nscapi::test_helpers::stub_core &core() { return nscapi::test_helpers::stub_core::instance(); }
  void SetUp() override {
    core().reset();
    proxy_ = nscapi::settings_proxy::create(7, nscapi::plugin_singleton->get_core());
  }
  nscapi::settings_proxy::ptr proxy_;
};

const std::string path = "/settings/test/section";

}  // namespace

TEST_F(SettingsProxy, GetStringReadsTheValueOrTheDefault) {
  core().set_setting(path, "host", "example.com");
  EXPECT_EQ(proxy_->get_string(path, "host", "def"), "example.com");
  EXPECT_EQ(proxy_->get_string(path, "missing", "def"), "def");
}

TEST_F(SettingsProxy, GetIntParsesAndFallsBack) {
  core().set_setting(path, "port", "5666");
  core().set_setting(path, "bad", "five");
  EXPECT_EQ(proxy_->get_int(path, "port", 1), 5666);
  EXPECT_EQ(proxy_->get_int(path, "missing", 17), 17);
  EXPECT_EQ(proxy_->get_int(path, "bad", 17), 17);
  EXPECT_EQ(proxy_->get_int(path, "missing", -3), -3);
}

TEST_F(SettingsProxy, GetBoolParsesAndFallsBack) {
  core().set_setting(path, "on", "true");
  core().set_setting(path, "yes", "yes");
  core().set_setting(path, "off", "false");
  EXPECT_TRUE(proxy_->get_bool(path, "on", false));
  EXPECT_TRUE(proxy_->get_bool(path, "yes", false));
  EXPECT_FALSE(proxy_->get_bool(path, "off", true));
  EXPECT_TRUE(proxy_->get_bool(path, "missing", true));
  EXPECT_FALSE(proxy_->get_bool(path, "missing", false));
}

TEST_F(SettingsProxy, SettersSendTheValueAsAString) {
  proxy_->set_string(path, "s", "text");
  proxy_->set_int(path, "i", -42);
  proxy_->set_bool(path, "b", true);
  proxy_->set_bool(path, "f", false);
  EXPECT_EQ(core().updated_value("s"), "text");
  EXPECT_EQ(core().updated_value("i"), "-42");
  EXPECT_EQ(core().updated_value("b"), "true");
  EXPECT_EQ(core().updated_value("f"), "false");
  ASSERT_EQ(core().updated_settings().size(), 4u);
  EXPECT_EQ(core().updated_settings()[0].path, path);
}

// Removal is an update without a value (and, for a whole path, without a
// key); the core's settings_query_handler reads it that way. The flip side:
// set_string(path, key, "") is the same request, so it removes the key
// rather than storing an empty value.
TEST_F(SettingsProxy, RemoveIsAnUpdateWithoutAValue) {
  proxy_->remove_key(path, "gone");
  proxy_->remove_path(path + "/sub");
  proxy_->set_string(path, "emptied", "");
  const auto &u = core().updated_settings();
  ASSERT_EQ(u.size(), 3u);
  EXPECT_EQ(u[0].key, "gone");
  EXPECT_EQ(u[0].default_value, "");
  EXPECT_EQ(u[1].path, path + "/sub");
  EXPECT_EQ(u[1].key, "");
  EXPECT_EQ(u[2].key, "emptied");
  EXPECT_EQ(u[2].default_value, "");
}

// The core answers with full section paths; the proxy hands back the part
// below the queried path.
TEST_F(SettingsProxy, GetSectionsStripsTheQueriedPath) {
  core().set_sections(path, {"alpha", "beta"});
  const auto sections = proxy_->get_sections(path);
  EXPECT_EQ(sections, (nscapi::settings_proxy::string_list{"alpha", "beta"}));
  EXPECT_TRUE(proxy_->get_sections("/settings/none").empty());
}

TEST_F(SettingsProxy, GetKeysListsTheKeyNames) {
  core().set_keys(path, {{"a", "1"}, {"b", "2"}});
  EXPECT_EQ(proxy_->get_keys(path), (nscapi::settings_proxy::string_list{"a", "b"}));
  EXPECT_TRUE(proxy_->get_keys("/settings/none").empty());
}

TEST_F(SettingsProxy, RegisterKeyCarriesItsMetadata) {
  proxy_->register_key(path, "password", "password", "PASSWORD", "The secret", "changeme", true, false, true);
  ASSERT_TRUE(core().has_key(path, "password"));
  const auto &k = core().registered_keys().front();
  EXPECT_EQ(k.type, "password");
  EXPECT_EQ(k.title, "PASSWORD");
  EXPECT_EQ(k.description, "The secret");
  EXPECT_EQ(k.default_value, "changeme");
  EXPECT_TRUE(k.advanced);
  EXPECT_FALSE(k.sample);
  EXPECT_TRUE(k.sensitive);
}

TEST_F(SettingsProxy, RegisterPathsSubkeysAndTemplates) {
  proxy_->register_path(path, "Section", "A section", false, false);
  proxy_->register_subkey(path + "/sub", "Sub", "A subkey", false, true);
  proxy_->register_tpl(path + "/tpl", "Tpl", "icon", "A template", "{}");
  proxy_->register_user_keys(path + "/user");
  const auto &p = core().registered_paths();
  ASSERT_EQ(p.size(), 4u);
  EXPECT_EQ(p[0], path);
  EXPECT_EQ(p[1], path + "/sub");
  EXPECT_EQ(p[2], path + "/tpl");
  EXPECT_EQ(p[3], path + "/user");
  EXPECT_TRUE(core().registered_keys().empty());
}

TEST_F(SettingsProxy, ExpandPathGoesThroughTheCore) { EXPECT_EQ(proxy_->expand_path("/opt/nscp/${x}"), "/opt/nscp/${x}"); }

// Save and the log forwarders must not throw on a core that has no logger.
TEST_F(SettingsProxy, SaveAndLoggingAreSafeWithoutALogger) {
  EXPECT_NO_THROW(proxy_->save());
  EXPECT_NO_THROW(proxy_->save("ini://x.ini"));
  EXPECT_NO_THROW(proxy_->err(__FILE__, __LINE__, "e"));
  EXPECT_NO_THROW(proxy_->warn(__FILE__, __LINE__, "w"));
  EXPECT_NO_THROW(proxy_->info(__FILE__, __LINE__, "i"));
  EXPECT_NO_THROW(proxy_->debug(__FILE__, __LINE__, "d"));
}

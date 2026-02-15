/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtest/gtest.h>

#include <boost/make_shared.hpp>
#include <map>
#include <nscapi/nscapi_settings_helper.hpp>
#include <string>
#include <vector>

namespace nscapi {
namespace settings_helper {

// ============================================================================
// Mock implementation of settings_impl_interface for testing
// ============================================================================

class MockSettingsInterface : public settings_impl_interface {
 public:
  virtual ~MockSettingsInterface() = default;

  // Storage for registered items
  struct RegisteredPath {
    std::string path;
    std::string title;
    std::string description;
    bool advanced;
    bool sample;
  };

  struct RegisteredKey {
    std::string path;
    std::string key;
    std::string type;
    std::string title;
    std::string description;
    std::string defValue;
    bool advanced;
    bool sample;
    bool sensitive;
  };

  struct RegisteredSubkey {
    std::string path;
    std::string title;
    std::string description;
    bool advanced;
    bool sample;
  };

  struct RegisteredTpl {
    std::string path;
    std::string title;
    std::string icon;
    std::string description;
    std::string fields;
  };

  std::vector<RegisteredPath> registered_paths;
  std::vector<RegisteredKey> registered_keys;
  std::vector<RegisteredSubkey> registered_subkeys;
  std::vector<RegisteredTpl> registered_tpls;
  std::map<std::string, std::map<std::string, std::string>> settings_data;
  std::map<std::string, string_list> sections_data;
  std::map<std::string, string_list> keys_data;
  std::vector<std::string> error_messages;
  std::vector<std::string> warning_messages;
  std::vector<std::string> info_messages;
  std::vector<std::string> debug_messages;
  std::string expand_path_prefix;

  void register_path(std::string path, std::string title, std::string description, bool advanced, bool sample) override {
    registered_paths.push_back({path, title, description, advanced, sample});
  }

  void register_key(std::string path, std::string key, std::string type, std::string title, std::string description, std::string defValue, bool advanced,
                    bool sample, bool sensitive) override {
    registered_keys.push_back({path, key, type, title, description, defValue, advanced, sample, sensitive});
  }

  void register_subkey(std::string path, std::string title, std::string description, bool advanced, bool sample) override {
    registered_subkeys.push_back({path, title, description, advanced, sample});
  }

  void register_tpl(std::string path, std::string title, std::string icon, std::string description, std::string fields) override {
    registered_tpls.push_back({path, title, icon, description, fields});
  }

  std::string get_string(std::string path, std::string key, std::string def) override {
    const auto path_it = settings_data.find(path);
    if (path_it != settings_data.end()) {
      const auto key_it = path_it->second.find(key);
      if (key_it != path_it->second.end()) {
        return key_it->second;
      }
    }
    return def;
  }

  void set_string(std::string path, std::string key, std::string value) override { settings_data[path][key] = value; }

  string_list get_sections(const std::string path) override {
    const auto it = sections_data.find(path);
    if (it != sections_data.end()) {
      return it->second;
    }
    return {};
  }

  string_list get_keys(std::string path) override {
    const auto it = keys_data.find(path);
    if (it != keys_data.end()) {
      return it->second;
    }
    return {};
  }

  std::string expand_path(std::string key) override { return expand_path_prefix + key; }

  void remove_key(std::string path, std::string key) override {
    const auto path_it = settings_data.find(path);
    if (path_it != settings_data.end()) {
      path_it->second.erase(key);
    }
  }

  void remove_path(std::string path) override { settings_data.erase(path); }

  void err(const char* file, int line, std::string message) override { error_messages.push_back(message); }

  void warn(const char* file, int line, std::string message) override { warning_messages.push_back(message); }

  void info(const char* file, int line, std::string message) override { info_messages.push_back(message); }

  void debug(const char* file, int line, std::string message) override { debug_messages.push_back(message); }

  // Helper methods for tests
  void set_setting(const std::string& path, const std::string& key, const std::string& value) { settings_data[path][key] = value; }

  void set_keys(const std::string& path, const string_list& keys) { keys_data[path] = keys; }

  void set_sections(const std::string& path, const string_list& sections) { sections_data[path] = sections; }

  void clear() {
    registered_paths.clear();
    registered_keys.clear();
    registered_subkeys.clear();
    registered_tpls.clear();
    settings_data.clear();
    sections_data.clear();
    keys_data.clear();
    error_messages.clear();
    warning_messages.clear();
    info_messages.clear();
    debug_messages.clear();
    expand_path_prefix.clear();
  }
};

// ============================================================================
// Key Factory Function Tests
// ============================================================================

class KeyFactoryTest : public ::testing::Test {
 protected:
  boost::shared_ptr<MockSettingsInterface> mock_;

  void SetUp() override { mock_ = boost::make_shared<MockSettingsInterface>(); }

  void TearDown() override { mock_->clear(); }
};

TEST_F(KeyFactoryTest, StringKeyWithDefault) {
  std::string value;
  const auto key = string_key(&value, "default_value");
  EXPECT_EQ("default_value", key->get_default());
}

TEST_F(KeyFactoryTest, StringKeyWithoutDefault) {
  std::string value;
  const auto key = string_key(&value);
  EXPECT_EQ("", key->get_default());
}

TEST_F(KeyFactoryTest, IntKeyWithDefault) {
  int value = 0;
  const auto key = int_key(&value, 42);
  EXPECT_EQ("42", key->get_default());
}

TEST_F(KeyFactoryTest, IntKeyWithoutDefault) {
  int value = 0;
  const auto key = int_key(&value);
  EXPECT_EQ("0", key->get_default());
}

TEST_F(KeyFactoryTest, BoolKeyWithDefaultTrue) {
  bool value = false;
  const auto key = bool_key(&value, true);
  EXPECT_EQ("true", key->get_default());
}

TEST_F(KeyFactoryTest, BoolKeyWithDefaultFalse) {
  bool value = true;
  const auto key = bool_key(&value, false);
  EXPECT_EQ("false", key->get_default());
}

TEST_F(KeyFactoryTest, UintKeyWithDefault) {
  unsigned int value = 0;
  const auto key = uint_key(&value, 100u);
  EXPECT_EQ("100", key->get_default());
}

TEST_F(KeyFactoryTest, SizeKeyWithDefault) {
  std::size_t value = 0;
  const auto key = size_key(&value, 1024);
  EXPECT_EQ("1024", key->get_default());
}

TEST_F(KeyFactoryTest, PathKeyWithDefault) {
  std::string value;
  const auto key = path_key(&value, "/default/path");
  EXPECT_EQ("/default/path", key->get_default());
}

// ============================================================================
// Key Notify Tests
// ============================================================================

class KeyNotifyTest : public ::testing::Test {
 protected:
  boost::shared_ptr<MockSettingsInterface> mock_;

  void SetUp() override { mock_ = boost::shared_ptr<MockSettingsInterface>(new MockSettingsInterface()); }

  void TearDown() override { mock_->clear(); }
};

TEST_F(KeyNotifyTest, StringKeyNotifyWithValue) {
  std::string value;
  const auto key = string_key(&value, "default");
  mock_->set_setting("/test/path", "mykey", "actual_value");

  key->notify(mock_, "/test/path", "mykey");

  EXPECT_EQ("actual_value", value);
}

TEST_F(KeyNotifyTest, StringKeyNotifyWithDefault) {
  std::string value;
  const auto key = string_key(&value, "default");

  key->notify(mock_, "/test/path", "mykey");

  EXPECT_EQ("default", value);
}

TEST_F(KeyNotifyTest, IntKeyNotifyWithValue) {
  int value = 0;
  const auto key = int_key(&value, 10);
  mock_->set_setting("/test/path", "mykey", "42");

  key->notify(mock_, "/test/path", "mykey");

  EXPECT_EQ(42, value);
}

TEST_F(KeyNotifyTest, IntKeyNotifyWithDefault) {
  int value = 0;
  const auto key = int_key(&value, 10);

  key->notify(mock_, "/test/path", "mykey");

  EXPECT_EQ(10, value);
}

TEST_F(KeyNotifyTest, BoolKeyNotifyWithTrueValue) {
  bool value = false;
  const auto key = bool_key(&value, false);
  mock_->set_setting("/test/path", "mykey", "true");

  key->notify(mock_, "/test/path", "mykey");

  EXPECT_TRUE(value);
}

TEST_F(KeyNotifyTest, BoolKeyNotifyWithFalseValue) {
  bool value = true;
  const auto key = bool_key(&value, true);
  mock_->set_setting("/test/path", "mykey", "false");

  key->notify(mock_, "/test/path", "mykey");

  EXPECT_FALSE(value);
}

TEST_F(KeyNotifyTest, BoolKeyNotifyWith1Value) {
  bool value = false;
  const auto key = bool_key(&value, false);
  mock_->set_setting("/test/path", "mykey", "1");

  key->notify(mock_, "/test/path", "mykey");

  EXPECT_TRUE(value);
}

TEST_F(KeyNotifyTest, BoolKeyNotifyWith0Value) {
  bool value = true;
  const auto key = bool_key(&value, true);
  mock_->set_setting("/test/path", "mykey", "0");

  key->notify(mock_, "/test/path", "mykey");

  EXPECT_FALSE(value);
}

TEST_F(KeyNotifyTest, PathKeyExpandsPath) {
  std::string value;
  const auto key = path_key(&value, "/default");
  mock_->expand_path_prefix = "/expanded/";
  mock_->set_setting("/test/path", "mykey", "relative/path");

  key->notify(mock_, "/test/path", "mykey");

  EXPECT_EQ("/expanded/relative/path", value);
}

// ============================================================================
// Function Key Tests
// ============================================================================

class FunctionKeyTest : public ::testing::Test {
 protected:
  boost::shared_ptr<MockSettingsInterface> mock_;

  void SetUp() override { mock_ = boost::make_shared<MockSettingsInterface>(); }

  void TearDown() override { mock_->clear(); }
};

TEST_F(FunctionKeyTest, StringFunKeyNotify) {
  std::string captured_value;
  const auto key = string_fun_key([&captured_value](const std::string& val) { captured_value = val; }, "default");
  mock_->set_setting("/test/path", "mykey", "callback_value");

  key->notify(mock_, "/test/path", "mykey");

  EXPECT_EQ("callback_value", captured_value);
}

TEST_F(FunctionKeyTest, IntFunKeyNotify) {
  int captured_value = 0;
  const auto key = int_fun_key([&captured_value](const int val) { captured_value = val; }, 10);
  mock_->set_setting("/test/path", "mykey", "99");

  key->notify(mock_, "/test/path", "mykey");

  EXPECT_EQ(99, captured_value);
}

TEST_F(FunctionKeyTest, BoolFunKeyNotify) {
  bool captured_value = false;
  const auto key = bool_fun_key([&captured_value](const bool val) { captured_value = val; }, false);
  mock_->set_setting("/test/path", "mykey", "true");

  key->notify(mock_, "/test/path", "mykey");

  EXPECT_TRUE(captured_value);
}

TEST_F(FunctionKeyTest, PathFunKeyNotify) {
  std::string captured_value;
  const auto key = path_fun_key([&captured_value](const std::string& val) { captured_value = val; }, "/default");
  mock_->expand_path_prefix = "/base/";
  mock_->set_setting("/test/path", "mykey", "subpath");

  key->notify(mock_, "/test/path", "mykey");

  EXPECT_EQ("/base/subpath", captured_value);
}

// ============================================================================
// Map and KVP Path Tests
// ============================================================================

class MapPathTest : public ::testing::Test {
 protected:
  boost::shared_ptr<MockSettingsInterface> mock_;

  void SetUp() override { mock_ = boost::shared_ptr<MockSettingsInterface>(new MockSettingsInterface()); }

  void TearDown() override { mock_->clear(); }
};

TEST_F(MapPathTest, StringMapPathNotify) {
  std::map<std::string, std::string> map_value;
  const auto key = string_map_path(&map_value);

  mock_->set_keys("/test/path", {"key1", "key2"});
  mock_->set_setting("/test/path", "key1", "value1");
  mock_->set_setting("/test/path", "key2", "value2");

  key->notify_path(mock_, "/test/path");

  EXPECT_EQ(2u, map_value.size());
  EXPECT_EQ("value1", map_value["key1"]);
  EXPECT_EQ("value2", map_value["key2"]);
}

TEST_F(MapPathTest, FunValuesPathNotify) {
  std::map<std::string, std::string> captured_values;
  const auto key = fun_values_path([&captured_values](const std::string& k, const std::string& v) { captured_values[k] = v; });

  mock_->set_keys("/test/path", {"key1", "key2"});
  mock_->set_setting("/test/path", "key1", "value1");
  mock_->set_setting("/test/path", "key2", "value2");

  key->notify_path(mock_, "/test/path");

  EXPECT_EQ(2u, captured_values.size());
  EXPECT_EQ("value1", captured_values["key1"]);
  EXPECT_EQ("value2", captured_values["key2"]);
}

TEST_F(MapPathTest, StringMapPathWithSections) {
  std::map<std::string, std::string> map_value;
  const auto key = string_map_path(&map_value);

  mock_->set_keys("/test/path", {"key1"});
  mock_->set_sections("/test/path", {"section1", "section2"});
  mock_->set_setting("/test/path", "key1", "value1");

  key->notify_path(mock_, "/test/path");

  // Keys with values are stored
  EXPECT_EQ("value1", map_value["key1"]);
  // Sections are stored with empty values, but map_storer ignores empty values
  EXPECT_EQ(1u, map_value.size());
}

// ============================================================================
// Settings Registry Tests
// ============================================================================

class SettingsRegistryTest : public ::testing::Test {
 protected:
  boost::shared_ptr<MockSettingsInterface> mock_;

  void SetUp() override { mock_ = boost::make_shared<MockSettingsInterface>(); }

  void TearDown() override { mock_->clear(); }
};

TEST_F(SettingsRegistryTest, RegisterStringKey) {
  settings_registry registry(mock_);
  std::string value;

  registry.add_key_to_path("/test/path").add_string("mykey", string_key(&value, "default"), "Title", "Description");

  registry.register_all();

  ASSERT_EQ(1u, mock_->registered_keys.size());
  EXPECT_EQ("/test/path", mock_->registered_keys[0].path);
  EXPECT_EQ("mykey", mock_->registered_keys[0].key);
  EXPECT_EQ("string", mock_->registered_keys[0].type);
  EXPECT_EQ("Title", mock_->registered_keys[0].title);
  EXPECT_EQ("Description", mock_->registered_keys[0].description);
  EXPECT_EQ("default", mock_->registered_keys[0].defValue);
}

TEST_F(SettingsRegistryTest, RegisterBoolKey) {
  settings_registry registry(mock_);
  bool value = false;

  registry.add_key_to_path("/test/path").add_bool("enabled", bool_key(&value, true), "Enabled", "Enable feature");

  registry.register_all();

  ASSERT_EQ(1u, mock_->registered_keys.size());
  EXPECT_EQ("bool", mock_->registered_keys[0].type);
  EXPECT_EQ("true", mock_->registered_keys[0].defValue);
}

TEST_F(SettingsRegistryTest, RegisterIntKey) {
  settings_registry registry(mock_);
  int value = 0;

  registry.add_key_to_path("/test/path").add_int("count", int_key(&value, 100), "Count", "Number of items");

  registry.register_all();

  ASSERT_EQ(1u, mock_->registered_keys.size());
  EXPECT_EQ("int", mock_->registered_keys[0].type);
  EXPECT_EQ("100", mock_->registered_keys[0].defValue);
}

TEST_F(SettingsRegistryTest, RegisterFileKey) {
  settings_registry registry(mock_);
  std::string value;

  registry.add_key_to_path("/test/path").add_file("config", path_key(&value, "/etc/config"), "Config File", "Path to config file");

  registry.register_all();

  ASSERT_EQ(1u, mock_->registered_keys.size());
  EXPECT_EQ("file", mock_->registered_keys[0].type);
}

TEST_F(SettingsRegistryTest, RegisterPasswordKey) {
  settings_registry registry(mock_);
  std::string value;

  registry.add_key_to_path("/test/path").add_password("secret", string_key(&value, ""), "Password", "Secret password");

  registry.register_all();

  ASSERT_EQ(1u, mock_->registered_keys.size());
  EXPECT_EQ("password", mock_->registered_keys[0].type);
  EXPECT_TRUE(mock_->registered_keys[0].sensitive);
}

TEST_F(SettingsRegistryTest, RegisterPath) {
  settings_registry registry(mock_);

  registry.add_path_to_settings()("test", "Test Path", "Test path description");

  registry.register_all();

  ASSERT_EQ(1u, mock_->registered_paths.size());
  EXPECT_EQ("/settings/test", mock_->registered_paths[0].path);
  EXPECT_EQ("Test Path", mock_->registered_paths[0].title);
  EXPECT_EQ("Test path description", mock_->registered_paths[0].description);
}

TEST_F(SettingsRegistryTest, NotifyKey) {
  settings_registry registry(mock_);
  std::string value;

  registry.add_key_to_path("/test/path").add_string("mykey", string_key(&value, "default"), "Title", "Description");

  mock_->set_setting("/test/path", "mykey", "actual_value");
  registry.notify();

  EXPECT_EQ("actual_value", value);
}

TEST_F(SettingsRegistryTest, NotifyMultipleKeys) {
  settings_registry registry(mock_);
  std::string str_value;
  int int_value = 0;
  bool bool_value = false;

  registry.add_key_to_path("/test/path")
      .add_string("str_key", string_key(&str_value, "str_default"), "String", "String key")
      .add_int("int_key", int_key(&int_value, 10), "Int", "Int key")
      .add_bool("bool_key", bool_key(&bool_value, false), "Bool", "Bool key");

  mock_->set_setting("/test/path", "str_key", "hello");
  mock_->set_setting("/test/path", "int_key", "42");
  mock_->set_setting("/test/path", "bool_key", "true");

  registry.notify();

  EXPECT_EQ("hello", str_value);
  EXPECT_EQ(42, int_value);
  EXPECT_TRUE(bool_value);
}

TEST_F(SettingsRegistryTest, ClearRegistry) {
  settings_registry registry(mock_);
  std::string value;

  registry.add_key_to_path("/test/path").add_string("mykey", string_key(&value, "default"), "Title", "Description");

  registry.clear();
  registry.register_all();

  EXPECT_EQ(0u, mock_->registered_keys.size());
}

TEST_F(SettingsRegistryTest, SetStaticKey) {
  const settings_registry registry(mock_);

  registry.set_static_key("/test/path", "mykey", "static_value");

  EXPECT_EQ("static_value", mock_->get_string("/test/path", "mykey", ""));
}

TEST_F(SettingsRegistryTest, GetStaticString) {
  const settings_registry registry(mock_);
  mock_->set_setting("/test/path", "mykey", "stored_value");

  const auto result = registry.get_static_string("/test/path", "mykey", "default");

  EXPECT_EQ("stored_value", result);
}

TEST_F(SettingsRegistryTest, ExpandPath) {
  const settings_registry registry(mock_);
  mock_->expand_path_prefix = "/expanded/";

  const auto result = registry.expand_path("relative/path");

  EXPECT_EQ("/expanded/relative/path", result);
}

// ============================================================================
// Alias Extension Tests
// ============================================================================

class AliasExtensionTest : public ::testing::Test {
 protected:
  boost::shared_ptr<MockSettingsInterface> mock_;

  void SetUp() override { mock_ = boost::make_shared<MockSettingsInterface>(); }

  void TearDown() override { mock_->clear(); }
};

TEST_F(AliasExtensionTest, GetAliasWithCurrent) {
  const auto result = alias_extension::get_alias("current", "default");
  EXPECT_EQ("current", result);
}

TEST_F(AliasExtensionTest, GetAliasWithEmpty) {
  const auto result = alias_extension::get_alias("", "default");
  EXPECT_EQ("default", result);
}

TEST_F(AliasExtensionTest, GetAliasWithPrefix) {
  const auto result = alias_extension::get_alias("prefix", "current", "default");
  EXPECT_EQ("prefix/current", result);
}

TEST_F(AliasExtensionTest, GetAliasWithPrefixAndEmptyCurrent) {
  const auto result = alias_extension::get_alias("prefix", "", "default");
  EXPECT_EQ("prefix/default", result);
}

TEST_F(AliasExtensionTest, GetPath) {
  settings_registry registry(mock_);
  const auto ext = registry.alias("myalias");

  EXPECT_EQ("/myalias", ext.get_path());
  EXPECT_EQ("custom/myalias", ext.get_path("custom"));
}

TEST_F(AliasExtensionTest, GetSettingsPath) {
  settings_registry registry(mock_);
  const auto ext = registry.alias("myalias");

  EXPECT_EQ("/settings/myalias", ext.get_settings_path(""));
  EXPECT_EQ("/settings/myalias/sub", ext.get_settings_path("sub"));
}

// ============================================================================
// Path Extension Tests
// ============================================================================

class PathExtensionTest : public ::testing::Test {
 protected:
  boost::shared_ptr<MockSettingsInterface> mock_;

  void SetUp() override { mock_ = boost::make_shared<MockSettingsInterface>(); }

  void TearDown() override { mock_->clear(); }
};

TEST_F(PathExtensionTest, GetPath) {
  settings_registry registry(mock_);
  auto ext = registry.path("/base/path");

  EXPECT_EQ("/base/path", ext.get_path(""));
  EXPECT_EQ("/base/path/sub", ext.get_path("sub"));
}

// ============================================================================
// Description Container Tests
// ============================================================================

class DescriptionContainerTest : public ::testing::Test {};

TEST_F(DescriptionContainerTest, DefaultConstructor) {
  const description_container desc;
  EXPECT_EQ(key_type_path, desc.type);
  EXPECT_FALSE(desc.advanced);
  EXPECT_EQ("", desc.title);
  EXPECT_EQ("", desc.description);
}

TEST_F(DescriptionContainerTest, ConstructorWithAdvanced) {
  const description_container desc(key_type_string, "Title", "Description", true);
  EXPECT_EQ(key_type_string, desc.type);
  EXPECT_EQ("Title", desc.title);
  EXPECT_EQ("Description", desc.description);
  EXPECT_TRUE(desc.advanced);
}

TEST_F(DescriptionContainerTest, ConstructorWithIcon) {
  const description_container desc(key_type_template, "Title", "Description", false, "icon.png");
  EXPECT_EQ(key_type_template, desc.type);
  EXPECT_EQ("Title", desc.title);
  EXPECT_EQ("Description", desc.description);
  EXPECT_EQ("icon.png", desc.icon);
  EXPECT_FALSE(desc.advanced);
}

TEST_F(DescriptionContainerTest, CopyConstructor) {
  const description_container desc1(key_type_int, "Title", "Description", true);
  const description_container desc2(desc1);
  EXPECT_EQ(desc1.type, desc2.type);
  EXPECT_EQ(desc1.title, desc2.title);
  EXPECT_EQ(desc1.description, desc2.description);
  EXPECT_EQ(desc1.advanced, desc2.advanced);
}

TEST_F(DescriptionContainerTest, AssignmentOperator) {
  const description_container desc1(key_type_bool, "Title1", "Description1", false);
  description_container desc2(key_type_int, "Title2", "Description2", true);
  desc2 = desc1;
  EXPECT_EQ(desc1.type, desc2.type);
  EXPECT_EQ(desc1.title, desc2.title);
  EXPECT_EQ(desc1.description, desc2.description);
  EXPECT_EQ(desc1.advanced, desc2.advanced);
}

// ============================================================================
// Edge Case Tests
// ============================================================================

class SettingsHelperEdgeCaseTest : public ::testing::Test {
 protected:
  boost::shared_ptr<MockSettingsInterface> mock_;

  void SetUp() override { mock_ = boost::make_shared<MockSettingsInterface>(); }

  void TearDown() override { mock_->clear(); }
};

TEST_F(SettingsHelperEdgeCaseTest, EmptyPath) {
  settings_registry registry(mock_);
  std::string value;

  registry.add_key_to_path("").add_string("mykey", string_key(&value, "default"), "Title", "Description");

  registry.register_all();

  ASSERT_EQ(1u, mock_->registered_keys.size());
  EXPECT_EQ("", mock_->registered_keys[0].path);
}

TEST_F(SettingsHelperEdgeCaseTest, EmptyKey) {
  settings_registry registry(mock_);
  std::string value;

  registry.add_key_to_path("/test/path").add_string("", string_key(&value, "default"), "Title", "Description");

  registry.register_all();

  ASSERT_EQ(1u, mock_->registered_keys.size());
  EXPECT_EQ("", mock_->registered_keys[0].key);
}

TEST_F(SettingsHelperEdgeCaseTest, NullPointerString) {
  // Test that null pointer is handled gracefully
  const auto key = string_key(nullptr, "default");
  EXPECT_EQ("default", key->get_default());

  // Notify should not crash with null pointer
  key->notify(mock_, "/test/path", "mykey");
  // No crash means success
  SUCCEED();
}

TEST_F(SettingsHelperEdgeCaseTest, NullPointerInt) {
  const auto key = int_key(nullptr, 42);
  EXPECT_EQ("42", key->get_default());

  key->notify(mock_, "/test/path", "mykey");
  SUCCEED();
}

TEST_F(SettingsHelperEdgeCaseTest, NullPointerBool) {
  const auto key = bool_key(nullptr, true);
  EXPECT_EQ("true", key->get_default());

  key->notify(mock_, "/test/path", "mykey");
  SUCCEED();
}

TEST_F(SettingsHelperEdgeCaseTest, SpecialCharactersInPath) {
  settings_registry registry(mock_);
  std::string value;

  registry.add_key_to_path("/path/with spaces/and-dashes").add_string("key_name", string_key(&value, "default"), "Title", "Description");

  registry.register_all();

  EXPECT_EQ("/path/with spaces/and-dashes", mock_->registered_keys[0].path);
}

TEST_F(SettingsHelperEdgeCaseTest, UnicodeInValues) {
  settings_registry registry(mock_);
  std::string value;

  registry.add_key_to_path("/test/path").add_string("mykey", string_key(&value, "default"), "Título", "Descripción con ñ y ü");

  mock_->set_setting("/test/path", "mykey", "valor con ñ");
  registry.notify();

  EXPECT_EQ("valor con ñ", value);
}

TEST_F(SettingsHelperEdgeCaseTest, LargeIntValue) {
  int value = 0;
  const auto key = int_key(&value, 2147483647);  // Max int
  mock_->set_setting("/test/path", "mykey", "2147483647");

  key->notify(mock_, "/test/path", "mykey");

  EXPECT_EQ(2147483647, value);
}

TEST_F(SettingsHelperEdgeCaseTest, NegativeIntValue) {
  int value = 0;
  const auto key = int_key(&value, -100);
  mock_->set_setting("/test/path", "mykey", "-500");

  key->notify(mock_, "/test/path", "mykey");

  EXPECT_EQ(-500, value);
}

TEST_F(SettingsHelperEdgeCaseTest, PathKeyWithParentAppliesExpansion) {
  // This test verifies that path_key applies expand_path even when using parent inheritance
  std::string value;
  const auto key = path_key(&value, "default_path");

  // Set up the mock to expand paths by prepending a prefix
  mock_->expand_path_prefix = "/expanded/";

  // Set setting in parent
  mock_->set_setting("/parent/path", "mykey", "original_path");
  // Set setting in child (should be used)
  mock_->set_setting("/child/path", "mykey", "child_path");

  // Use parent-aware notify (three parameters: parent, path, key)
  key->notify(mock_, "/parent/path", "/child/path", "mykey");

  // The value should be expanded via expand_path
  EXPECT_EQ("/expanded/child_path", value);
}

TEST_F(SettingsHelperEdgeCaseTest, PathKeyWithParentInheritsFromParent) {
  // Test that when child has no value, parent's value is used and expanded
  std::string value;
  const auto key = path_key(&value, "default_path");

  mock_->expand_path_prefix = "/expanded/";

  // Only set in parent, not in child
  mock_->set_setting("/parent/path", "mykey", "parent_value");

  key->notify(mock_, "/parent/path", "/child/path", "mykey");

  EXPECT_EQ("/expanded/parent_value", value);
}

}  // namespace settings_helper
}  // namespace nscapi

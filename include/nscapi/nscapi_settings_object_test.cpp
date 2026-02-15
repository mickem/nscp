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
#include <nscapi/nscapi_settings_object.hpp>
#include <string>
#include <vector>

namespace nscapi {
namespace settings_objects {

// ============================================================================
// Helper function tests
// ============================================================================

TEST(SettingsObjectHelpersTest, ImportStringEmptyObjectNonEmptyParent) {
  std::string object;
  const std::string parent = "parent_value";
  import_string(object, parent);
  EXPECT_EQ("parent_value", object);
}

TEST(SettingsObjectHelpersTest, ImportStringNonEmptyObjectIgnoresParent) {
  std::string object = "existing_value";
  const std::string parent = "parent_value";
  import_string(object, parent);
  EXPECT_EQ("existing_value", object);
}

TEST(SettingsObjectHelpersTest, ImportStringEmptyObjectEmptyParent) {
  std::string object;
  const std::string parent;
  import_string(object, parent);
  EXPECT_EQ("", object);
}

TEST(SettingsObjectHelpersTest, MakeObjPathBasic) { EXPECT_EQ("/settings/targets/myalias", make_obj_path("/settings/targets", "myalias")); }

TEST(SettingsObjectHelpersTest, MakeObjPathEmptyAlias) { EXPECT_EQ("/settings/targets/", make_obj_path("/settings/targets", "")); }

TEST(SettingsObjectHelpersTest, MakeObjPathEmptyBasePath) { EXPECT_EQ("/myalias", make_obj_path("", "myalias")); }

// ============================================================================
// Mock settings interface for testing
// ============================================================================

class MockSettingsInterface : public settings_helper::settings_impl_interface {
 public:
  virtual ~MockSettingsInterface() = default;

  std::map<std::string, std::map<std::string, std::string>> settings_data;
  std::map<std::string, string_list> sections_data;
  std::map<std::string, string_list> keys_data;
  std::vector<std::string> removed_paths;
  std::vector<std::pair<std::string, std::string>> removed_keys;

  void register_path(std::string path, std::string title, std::string description, bool advanced, bool sample) override {}
  void register_key(std::string path, std::string key, std::string type, std::string title, std::string description, std::string defValue, bool advanced,
                    bool sample, bool sensitive) override {}
  void register_subkey(std::string path, std::string title, std::string description, bool advanced, bool sample) override {}
  void register_tpl(std::string path, std::string title, std::string icon, std::string description, std::string fields) override {}

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

  std::string expand_path(std::string key) override { return key; }

  void remove_key(std::string path, std::string key) override { removed_keys.push_back({path, key}); }

  void remove_path(std::string path) override { removed_paths.push_back(path); }

  void err(const char* file, int line, std::string message) override {}
  void warn(const char* file, int line, std::string message) override {}
  void info(const char* file, int line, std::string message) override {}
  void debug(const char* file, int line, std::string message) override {}

  // Helper methods for tests
  void set_setting(const std::string& path, const std::string& key, const std::string& value) { settings_data[path][key] = value; }
  void set_keys(const std::string& path, const string_list& keys) { keys_data[path] = keys; }
  void set_sections(const std::string& path, const string_list& sections) { sections_data[path] = sections; }

  void clear() {
    settings_data.clear();
    sections_data.clear();
    keys_data.clear();
    removed_paths.clear();
    removed_keys.clear();
  }
};

// ============================================================================
// object_instance_interface tests
// ============================================================================

class ObjectInstanceInterfaceTest : public ::testing::Test {
 protected:
  boost::shared_ptr<MockSettingsInterface> mock_;

  void SetUp() override { mock_ = boost::make_shared<MockSettingsInterface>(); }

  void TearDown() override { mock_->clear(); }
};

TEST_F(ObjectInstanceInterfaceTest, ConstructorBasic) {
  const object_instance_interface obj("myalias", "/settings/targets");

  EXPECT_EQ("myalias", obj.get_alias());
  EXPECT_EQ("/settings/targets", obj.get_base_path());
  EXPECT_EQ("/settings/targets/myalias", obj.get_path());
  EXPECT_FALSE(obj.is_template());
  EXPECT_FALSE(obj.is_default());
}

TEST_F(ObjectInstanceInterfaceTest, ConstructorDefault) {
  const object_instance_interface obj("default", "/settings/targets");

  EXPECT_EQ("default", obj.get_alias());
  EXPECT_TRUE(obj.is_default());
}

TEST_F(ObjectInstanceInterfaceTest, CopyConstructorFromSharedPtr) {
  const auto parent = boost::make_shared<object_instance_interface>("parent_alias", "/settings/targets");
  parent->set_value("parent_value");
  parent->set_property_string("key1", "value1");
  parent->set_property_int("key2", 42);

  object_instance_interface child(parent, "child_alias", "/settings/targets");

  EXPECT_EQ("child_alias", child.get_alias());
  EXPECT_EQ("/settings/targets/child_alias", child.get_path());
  EXPECT_EQ("parent_value", child.get_value());
  EXPECT_EQ("value1", child.get_property_string("key1"));
  EXPECT_EQ("42", child.get_property_string("key2"));
}

TEST_F(ObjectInstanceInterfaceTest, CopyConstructor) {
  object_instance_interface original("original", "/base");
  original.set_value("my_value");
  original.set_property_string("opt1", "val1");
  original.make_template(true);

  object_instance_interface copy(original);

  EXPECT_EQ("original", copy.get_alias());
  EXPECT_EQ("/base", copy.get_base_path());
  EXPECT_EQ("my_value", copy.get_value());
  EXPECT_EQ("val1", copy.get_property_string("opt1"));
  EXPECT_TRUE(copy.is_template());
}

TEST_F(ObjectInstanceInterfaceTest, SetupChangesAliasAndPath) {
  object_instance_interface obj("old_alias", "/old/path");
  obj.setup("new_alias", "/new/path");

  EXPECT_EQ("new_alias", obj.get_alias());
  EXPECT_EQ("/new/path", obj.get_base_path());
  EXPECT_EQ("/new/path/new_alias", obj.get_path());
}

TEST_F(ObjectInstanceInterfaceTest, TemplateManagement) {
  object_instance_interface obj("myalias", "/settings");

  EXPECT_FALSE(obj.is_template());

  obj.make_template(true);
  EXPECT_TRUE(obj.is_template());

  obj.make_template(false);
  EXPECT_FALSE(obj.is_template());
}

TEST_F(ObjectInstanceInterfaceTest, ValueGetSet) {
  object_instance_interface obj("alias", "/path");

  EXPECT_EQ("", obj.get_value());

  obj.set_value("test_value");
  EXPECT_EQ("test_value", obj.get_value());
}

TEST_F(ObjectInstanceInterfaceTest, SetPropertyString) {
  object_instance_interface obj("alias", "/path");

  obj.set_property_string("mykey", "myvalue");
  EXPECT_TRUE(obj.has_option("mykey"));
  EXPECT_EQ("myvalue", obj.get_property_string("mykey"));
}

TEST_F(ObjectInstanceInterfaceTest, SetPropertyInt) {
  object_instance_interface obj("alias", "/path");

  obj.set_property_int("intkey", 12345);
  EXPECT_TRUE(obj.has_option("intkey"));
  EXPECT_EQ(12345, obj.get_property_int("intkey", 0));
}

TEST_F(ObjectInstanceInterfaceTest, SetPropertyBool) {
  object_instance_interface obj("alias", "/path");

  obj.set_property_bool("boolkey", true);
  EXPECT_TRUE(obj.has_option("boolkey"));
  EXPECT_TRUE(obj.get_property_bool("boolkey", false));

  obj.set_property_bool("boolkey2", false);
  EXPECT_FALSE(obj.get_property_bool("boolkey2", true));
}

TEST_F(ObjectInstanceInterfaceTest, GetPropertyStringDefault) {
  object_instance_interface obj("alias", "/path");

  EXPECT_EQ("default_val", obj.get_property_string("nonexistent", "default_val"));
  EXPECT_EQ("", obj.get_property_string("nonexistent"));
}

TEST_F(ObjectInstanceInterfaceTest, GetPropertyIntDefault) {
  object_instance_interface obj("alias", "/path");

  EXPECT_EQ(999, obj.get_property_int("nonexistent", 999));
}

TEST_F(ObjectInstanceInterfaceTest, GetPropertyBoolDefault) {
  object_instance_interface obj("alias", "/path");

  EXPECT_TRUE(obj.get_property_bool("nonexistent", true));
  EXPECT_FALSE(obj.get_property_bool("nonexistent", false));
}

TEST_F(ObjectInstanceInterfaceTest, HasOption) {
  object_instance_interface obj("alias", "/path");

  EXPECT_FALSE(obj.has_option("key"));
  obj.set_property_string("key", "value");
  EXPECT_TRUE(obj.has_option("key"));
}

TEST_F(ObjectInstanceInterfaceTest, TranslateAddsOption) {
  object_instance_interface obj("alias", "/path");

  obj.translate("translated_key", "translated_value");
  EXPECT_TRUE(obj.has_option("translated_key"));
  EXPECT_EQ("translated_value", obj.get_property_string("translated_key"));
}

TEST_F(ObjectInstanceInterfaceTest, GetOptions) {
  object_instance_interface obj("alias", "/path");

  obj.set_property_string("key1", "val1");
  obj.set_property_string("key2", "val2");

  const auto& options = obj.get_options();
  EXPECT_EQ(2u, options.size());
  EXPECT_NE(options.end(), options.find("key1"));
  EXPECT_NE(options.end(), options.find("key2"));
}

TEST_F(ObjectInstanceInterfaceTest, ToStringContainsAliasAndPath) {
  object_instance_interface obj("myalias", "/mypath");
  obj.set_value("myvalue");
  obj.set_property_string("opt1", "val1");

  std::string str = obj.to_string();
  EXPECT_NE(std::string::npos, str.find("myalias"));
  EXPECT_NE(std::string::npos, str.find("/mypath/myalias"));
  EXPECT_NE(std::string::npos, str.find("myvalue"));
  EXPECT_NE(std::string::npos, str.find("opt1"));
  EXPECT_NE(std::string::npos, str.find("val1"));
}

TEST_F(ObjectInstanceInterfaceTest, SetAlias) {
  object_instance_interface obj("old", "/path");
  obj.set_alias("new");
  EXPECT_EQ("new", obj.get_alias());
}

// ============================================================================
// simple_object_factory tests
// ============================================================================

TEST(SimpleObjectFactoryTest, CreateReturnsNewObject) {
  simple_object_factory<object_instance_interface> factory;

  const auto obj = factory.create("myalias", "/settings/targets");

  ASSERT_TRUE(obj != nullptr);
  EXPECT_EQ("myalias", obj->get_alias());
  EXPECT_EQ("/settings/targets", obj->get_base_path());
}

TEST(SimpleObjectFactoryTest, CloneCopiesParent) {
  simple_object_factory<object_instance_interface> factory;

  const auto parent = factory.create("parent", "/settings/targets");
  parent->set_value("parent_value");
  parent->set_property_string("key1", "value1");

  const auto child = factory.clone(parent, "child", "/settings/targets");

  ASSERT_TRUE(child != nullptr);
  EXPECT_EQ("child", child->get_alias());
  EXPECT_EQ("/settings/targets/child", child->get_path());
  EXPECT_EQ("parent_value", child->get_value());
  EXPECT_EQ("value1", child->get_property_string("key1"));
}

// ============================================================================
// object_handler tests
// ============================================================================

class ObjectHandlerTest : public ::testing::Test {
 protected:
  boost::shared_ptr<MockSettingsInterface> mock_;
  object_handler<object_instance_interface> handler_;

  void SetUp() override {
    mock_ = boost::make_shared<MockSettingsInterface>();
    handler_.set_path("/settings/targets");
  }

  void TearDown() override {
    mock_->clear();
    handler_.clear();
  }
};

TEST_F(ObjectHandlerTest, InitiallyEmpty) {
  EXPECT_TRUE(handler_.empty());
  EXPECT_FALSE(handler_.has_objects());
}

TEST_F(ObjectHandlerTest, SetPath) {
  object_handler<object_instance_interface> h;
  h.set_path("/custom/path");
  // Path is used internally for object creation
  const auto obj = h.add(nullptr, "test", "");
  EXPECT_EQ("/custom/path", obj->get_base_path());
}

TEST_F(ObjectHandlerTest, AddObjectWithoutProxy) {
  const auto obj = handler_.add(nullptr, "myalias", "myvalue");

  ASSERT_TRUE(obj != nullptr);
  EXPECT_EQ("myalias", obj->get_alias());
  EXPECT_EQ("myvalue", obj->get_value());
  EXPECT_FALSE(handler_.empty());
  EXPECT_TRUE(handler_.has_objects());
}

TEST_F(ObjectHandlerTest, AddObjectWithProxy) {
  mock_->set_keys("/settings/targets/myalias", {"key1", "key2"});

  const auto obj = handler_.add(mock_, "myalias", "myvalue");

  ASSERT_TRUE(obj != nullptr);
  EXPECT_EQ("myalias", obj->get_alias());
  EXPECT_FALSE(handler_.empty());
}

TEST_F(ObjectHandlerTest, AddDefaultCreatesTemplate) {
  const auto obj = handler_.add(nullptr, "default", "");

  ASSERT_TRUE(obj != nullptr);
  EXPECT_TRUE(obj->is_template());
  EXPECT_TRUE(handler_.has_object("default"));
  EXPECT_TRUE(handler_.empty());  // Templates don't count as objects
}

TEST_F(ObjectHandlerTest, AddWithForceTemplate) {
  const auto obj = handler_.add(nullptr, "myalias", "", true);

  ASSERT_TRUE(obj != nullptr);
  EXPECT_TRUE(obj->is_template());
  EXPECT_TRUE(handler_.empty());  // Templates don't count as objects
}

TEST_F(ObjectHandlerTest, AddDuplicateReturnsPrevious) {
  const auto first = handler_.add(nullptr, "myalias", "first_value");
  const auto second = handler_.add(nullptr, "myalias", "second_value");

  EXPECT_EQ(first.get(), second.get());
  EXPECT_EQ("first_value", second->get_value());
}

TEST_F(ObjectHandlerTest, HasObject) {
  EXPECT_FALSE(handler_.has_object("myalias"));

  handler_.add(nullptr, "myalias", "");
  EXPECT_TRUE(handler_.has_object("myalias"));
}

TEST_F(ObjectHandlerTest, HasObjectFindsTemplates) {
  handler_.add(nullptr, "default", "");
  EXPECT_TRUE(handler_.has_object("default"));
}

TEST_F(ObjectHandlerTest, FindObject) {
  handler_.add(nullptr, "myalias", "myvalue");

  const auto found = handler_.find_object("myalias");
  ASSERT_TRUE(found != nullptr);
  EXPECT_EQ("myalias", found->get_alias());
  EXPECT_EQ("myvalue", found->get_value());
}

TEST_F(ObjectHandlerTest, FindObjectReturnsNullForMissing) {
  const auto found = handler_.find_object("nonexistent");
  EXPECT_TRUE(found == nullptr);
}

TEST_F(ObjectHandlerTest, FindObjectFindsTemplates) {
  handler_.add(nullptr, "default", "default_value");

  const auto found = handler_.find_object("default");
  ASSERT_TRUE(found != nullptr);
  EXPECT_EQ("default", found->get_alias());
}

TEST_F(ObjectHandlerTest, RemoveByAlias) {
  handler_.add(nullptr, "myalias", "");
  EXPECT_TRUE(handler_.has_object("myalias"));

  const bool removed = handler_.remove("myalias");
  EXPECT_TRUE(removed);
  EXPECT_FALSE(handler_.has_object("myalias"));
}

TEST_F(ObjectHandlerTest, RemoveNonexistentReturnsFalse) {
  const bool removed = handler_.remove("nonexistent");
  EXPECT_FALSE(removed);
}

TEST_F(ObjectHandlerTest, RemoveTemplate) {
  handler_.add(nullptr, "default", "");
  EXPECT_TRUE(handler_.has_object("default"));

  const bool removed = handler_.remove("default");
  EXPECT_TRUE(removed);
  EXPECT_FALSE(handler_.has_object("default"));
}

TEST_F(ObjectHandlerTest, RemoveWithProxy) {
  handler_.add(nullptr, "myalias", "");

  const bool removed = handler_.remove(mock_, "myalias");

  EXPECT_TRUE(removed);
  EXPECT_FALSE(handler_.has_object("myalias"));
  EXPECT_EQ(1u, mock_->removed_paths.size());
  EXPECT_EQ("/settings/targets/myalias", mock_->removed_paths[0]);
  EXPECT_EQ(1u, mock_->removed_keys.size());
  EXPECT_EQ("/settings/targets", mock_->removed_keys[0].first);
  EXPECT_EQ("myalias", mock_->removed_keys[0].second);
}

TEST_F(ObjectHandlerTest, GetObjectList) {
  handler_.add(nullptr, "alias1", "value1");
  handler_.add(nullptr, "alias2", "value2");
  handler_.add(nullptr, "alias3", "value3");

  const auto list = handler_.get_object_list();
  EXPECT_EQ(3u, list.size());
}

TEST_F(ObjectHandlerTest, GetAliasList) {
  handler_.add(nullptr, "alias1", "");
  handler_.add(nullptr, "alias2", "");

  const auto aliases = handler_.get_alias_list();
  EXPECT_EQ(2u, aliases.size());

  bool found1 = false, found2 = false;
  for (const auto& a : aliases) {
    if (a == "alias1") found1 = true;
    if (a == "alias2") found2 = true;
  }
  EXPECT_TRUE(found1);
  EXPECT_TRUE(found2);
}

TEST_F(ObjectHandlerTest, Clear) {
  handler_.add(nullptr, "alias1", "");
  handler_.add(nullptr, "alias2", "");
  handler_.add(nullptr, "default", "");

  EXPECT_FALSE(handler_.empty());

  handler_.clear();

  EXPECT_TRUE(handler_.empty());
  EXPECT_FALSE(handler_.has_object("alias1"));
  EXPECT_FALSE(handler_.has_object("alias2"));
  EXPECT_FALSE(handler_.has_object("default"));
}

TEST_F(ObjectHandlerTest, AddMissingOnlyAddsIfNotExists) {
  handler_.add(nullptr, "existing", "original");

  handler_.add_missing(nullptr, "existing", "new_value");
  handler_.add_missing(nullptr, "new_alias", "new_value");

  const auto existing = handler_.find_object("existing");
  EXPECT_EQ("original", existing->get_value());

  const auto new_obj = handler_.find_object("new_alias");
  ASSERT_TRUE(new_obj != nullptr);
  EXPECT_EQ("new_value", new_obj->get_value());
}

TEST_F(ObjectHandlerTest, EnsureDefaultCreatesDefault) {
  EXPECT_FALSE(handler_.has_object("default"));

  handler_.ensure_default();

  EXPECT_TRUE(handler_.has_object("default"));
}

TEST_F(ObjectHandlerTest, EnsureDefaultDoesNotOverwrite) {
  handler_.add(nullptr, "default", "original_value");

  handler_.ensure_default();

  const auto def = handler_.find_object("default");
  EXPECT_EQ("original_value", def->get_value());
}

TEST_F(ObjectHandlerTest, AddObjectDirect) {
  const auto obj = boost::make_shared<object_instance_interface>("direct", "/settings/targets");
  obj->set_value("direct_value");

  handler_.add_object(obj);

  EXPECT_TRUE(handler_.has_object("direct"));
  const auto found = handler_.find_object("direct");
  EXPECT_EQ("direct_value", found->get_value());
}

TEST_F(ObjectHandlerTest, AddObjectDirectWithAlias) {
  const auto obj = boost::make_shared<object_instance_interface>("original", "/settings/targets");

  handler_.add_object("custom_alias", obj);

  EXPECT_TRUE(handler_.has_object("custom_alias"));
  EXPECT_FALSE(handler_.has_object("original"));
}

TEST_F(ObjectHandlerTest, AddTemplateDirect) {
  const auto obj = boost::make_shared<object_instance_interface>("template1", "/settings/targets");
  EXPECT_FALSE(obj->is_template());

  handler_.add_template(obj);

  EXPECT_TRUE(obj->is_template());
  EXPECT_TRUE(handler_.has_object("template1"));
  EXPECT_TRUE(handler_.empty());  // Templates don't count
}

TEST_F(ObjectHandlerTest, AddTemplateDirectWithAlias) {
  const auto obj = boost::make_shared<object_instance_interface>("original", "/settings/targets");

  handler_.add_template("custom_template", obj);

  EXPECT_TRUE(handler_.has_object("custom_template"));
}

TEST_F(ObjectHandlerTest, ToString) {
  handler_.add(nullptr, "obj1", "val1");
  handler_.add(nullptr, "default", "def_val");

  std::string str = handler_.to_string();

  EXPECT_NE(std::string::npos, str.find("Objects:"));
  EXPECT_NE(std::string::npos, str.find("Templates:"));
  EXPECT_NE(std::string::npos, str.find("obj1"));
  EXPECT_NE(std::string::npos, str.find("default"));
}

TEST_F(ObjectHandlerTest, AddWithParentInheritance) {
  // First add a parent template
  handler_.add(nullptr, "parent_template", "parent_value");
  const auto parent = handler_.find_object("parent_template");
  parent->set_property_string("inherited_key", "inherited_value");
  parent->make_template(true);

  // Configure mock to return parent name
  mock_->set_setting("/settings/targets/child", "parent", "parent_template");
  mock_->set_keys("/settings/targets/child", {"parent"});

  // Add child that inherits from parent
  const auto child = handler_.add(mock_, "child", "child_value");

  ASSERT_TRUE(child != nullptr);
  EXPECT_EQ("child", child->get_alias());
  // Child should inherit the option from parent
  EXPECT_EQ("inherited_value", child->get_property_string("inherited_key"));
}

TEST_F(ObjectHandlerTest, HasObjectsReturnsFalseForOnlyTemplates) {
  handler_.add(nullptr, "default", "");
  handler_.add(nullptr, "template1", "", true);

  EXPECT_FALSE(handler_.has_objects());
  EXPECT_TRUE(handler_.empty());
}

TEST_F(ObjectHandlerTest, HasObjectsReturnsTrueForObjects) {
  handler_.add(nullptr, "regular_object", "");

  EXPECT_TRUE(handler_.has_objects());
  EXPECT_FALSE(handler_.empty());
}

}  // namespace settings_objects
}  // namespace nscapi

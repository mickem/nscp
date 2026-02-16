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

#include <map>
#include <nscapi/protobuf/settings.hpp>
#include <settings/settings_value.hpp>
#include <string>
#include <vector>

namespace nscapi {

// ============================================================================
// settings_value tests (helper class used by proxy)
// ============================================================================

TEST(SettingsValueTest, FromIntPositive) { EXPECT_EQ("42", settings::settings_value::from_int(42)); }

TEST(SettingsValueTest, FromIntZero) { EXPECT_EQ("0", settings::settings_value::from_int(0)); }

TEST(SettingsValueTest, FromIntNegative) { EXPECT_EQ("-1", settings::settings_value::from_int(-1)); }

TEST(SettingsValueTest, ToIntValid) { EXPECT_EQ(42, settings::settings_value::to_int("42")); }

TEST(SettingsValueTest, ToIntZero) { EXPECT_EQ(0, settings::settings_value::to_int("0")); }

TEST(SettingsValueTest, ToIntNegative) { EXPECT_EQ(-5, settings::settings_value::to_int("-5")); }

TEST(SettingsValueTest, ToIntInvalidReturnsDefault) { EXPECT_EQ(-1, settings::settings_value::to_int("invalid")); }

TEST(SettingsValueTest, ToIntInvalidWithCustomDefault) { EXPECT_EQ(99, settings::settings_value::to_int("invalid", 99)); }

TEST(SettingsValueTest, FromBoolTrue) { EXPECT_EQ("true", settings::settings_value::from_bool(true)); }

TEST(SettingsValueTest, FromBoolFalse) { EXPECT_EQ("false", settings::settings_value::from_bool(false)); }

TEST(SettingsValueTest, ToBoolTrue) { EXPECT_TRUE(settings::settings_value::to_bool("true")); }

TEST(SettingsValueTest, ToBoolTrueUppercase) { EXPECT_TRUE(settings::settings_value::to_bool("TRUE")); }

TEST(SettingsValueTest, ToBoolTrueMixed) { EXPECT_TRUE(settings::settings_value::to_bool("True")); }

TEST(SettingsValueTest, ToBoolOne) { EXPECT_TRUE(settings::settings_value::to_bool("1")); }

TEST(SettingsValueTest, ToBoolYes) { EXPECT_TRUE(settings::settings_value::to_bool("yes")); }

TEST(SettingsValueTest, ToBoolYesUppercase) { EXPECT_TRUE(settings::settings_value::to_bool("YES")); }

TEST(SettingsValueTest, ToBoolFalse) { EXPECT_FALSE(settings::settings_value::to_bool("false")); }

TEST(SettingsValueTest, ToBoolZero) { EXPECT_FALSE(settings::settings_value::to_bool("0")); }

TEST(SettingsValueTest, ToBoolNo) { EXPECT_FALSE(settings::settings_value::to_bool("no")); }

TEST(SettingsValueTest, ToBoolEmpty) { EXPECT_FALSE(settings::settings_value::to_bool("")); }

TEST(SettingsValueTest, ToBoolInvalid) { EXPECT_FALSE(settings::settings_value::to_bool("invalid")); }

// ============================================================================
// Protobuf Settings Message Structure tests
// These test the message structures that proxy.cpp uses
// ============================================================================

TEST(SettingsRequestMessageTest, CreateRegistrationRequest) {
  PB::Settings::SettingsRequestMessage request;
  PB::Settings::SettingsRequestMessage::Request* payload = request.add_payload();
  payload->set_plugin_id(123);

  PB::Settings::SettingsRequestMessage::Request::Registration* regitem = payload->mutable_registration();
  regitem->mutable_node()->set_path("/settings/test");
  regitem->mutable_info()->set_title("Test Title");
  regitem->mutable_info()->set_description("Test Description");
  regitem->mutable_info()->set_advanced(true);
  regitem->mutable_info()->set_sample(false);
  regitem->mutable_info()->set_subkey(false);
  regitem->mutable_info()->set_is_sensitive(false);

  // Verify the structure
  EXPECT_EQ(1, request.payload_size());
  EXPECT_EQ(123, request.payload(0).plugin_id());
  EXPECT_TRUE(request.payload(0).has_registration());
  EXPECT_EQ("/settings/test", request.payload(0).registration().node().path());
  EXPECT_EQ("Test Title", request.payload(0).registration().info().title());
  EXPECT_EQ("Test Description", request.payload(0).registration().info().description());
  EXPECT_TRUE(request.payload(0).registration().info().advanced());
  EXPECT_FALSE(request.payload(0).registration().info().sample());
}

TEST(SettingsRequestMessageTest, CreateKeyRegistrationRequest) {
  PB::Settings::SettingsRequestMessage request;
  PB::Settings::SettingsRequestMessage::Request* payload = request.add_payload();
  payload->set_plugin_id(456);

  PB::Settings::SettingsRequestMessage::Request::Registration* regitem = payload->mutable_registration();
  regitem->mutable_node()->set_key("mykey");
  regitem->mutable_node()->set_path("/settings/mypath");
  regitem->mutable_info()->set_type("string");
  regitem->mutable_info()->set_title("Key Title");
  regitem->mutable_info()->set_description("Key Description");
  regitem->mutable_info()->set_default_value("default_value");
  regitem->mutable_info()->set_advanced(false);
  regitem->mutable_info()->set_sample(true);
  regitem->mutable_info()->set_is_sensitive(true);

  EXPECT_EQ(1, request.payload_size());
  EXPECT_EQ("/settings/mypath", request.payload(0).registration().node().path());
  EXPECT_EQ("mykey", request.payload(0).registration().node().key());
  EXPECT_EQ("string", request.payload(0).registration().info().type());
  EXPECT_EQ("default_value", request.payload(0).registration().info().default_value());
  EXPECT_TRUE(request.payload(0).registration().info().is_sensitive());
}

TEST(SettingsRequestMessageTest, CreateSubkeyRegistrationRequest) {
  PB::Settings::SettingsRequestMessage request;
  PB::Settings::SettingsRequestMessage::Request* payload = request.add_payload();
  payload->set_plugin_id(789);

  PB::Settings::SettingsRequestMessage::Request::Registration* regitem = payload->mutable_registration();
  regitem->mutable_node()->set_path("/settings/subkey/path");
  regitem->mutable_info()->set_title("Subkey Title");
  regitem->mutable_info()->set_description("Subkey Description");
  regitem->mutable_info()->set_advanced(true);
  regitem->mutable_info()->set_sample(true);
  regitem->mutable_info()->set_subkey(true);
  regitem->mutable_info()->set_is_sensitive(false);

  EXPECT_TRUE(request.payload(0).registration().info().subkey());
}

TEST(SettingsRequestMessageTest, CreateTemplateRegistrationRequest) {
  PB::Settings::SettingsRequestMessage request;
  PB::Settings::SettingsRequestMessage::Request* payload = request.add_payload();
  payload->set_plugin_id(111);

  PB::Settings::SettingsRequestMessage::Request::Registration* regitem = payload->mutable_registration();
  regitem->mutable_node()->set_path("/settings/templates/mytemplate");
  regitem->mutable_info()->set_icon("icon.png");
  regitem->mutable_info()->set_title("Template Title");
  regitem->mutable_info()->set_description("Template Description");
  regitem->set_fields("field1,field2,field3");

  EXPECT_EQ("icon.png", request.payload(0).registration().info().icon());
  EXPECT_EQ("field1,field2,field3", request.payload(0).registration().fields());
}

TEST(SettingsRequestMessageTest, CreateQueryRequest) {
  PB::Settings::SettingsRequestMessage request;
  PB::Settings::SettingsRequestMessage::Request* payload = request.add_payload();
  payload->set_plugin_id(222);

  PB::Settings::SettingsRequestMessage::Request::Query* item = payload->mutable_query();
  item->mutable_node()->set_key("testkey");
  item->mutable_node()->set_path("/settings/test");
  item->set_recursive(false);
  item->set_default_value("default");

  EXPECT_TRUE(request.payload(0).has_query());
  EXPECT_EQ("testkey", request.payload(0).query().node().key());
  EXPECT_EQ("/settings/test", request.payload(0).query().node().path());
  EXPECT_FALSE(request.payload(0).query().recursive());
  EXPECT_EQ("default", request.payload(0).query().default_value());
}

TEST(SettingsRequestMessageTest, CreateSectionsQueryRequest) {
  PB::Settings::SettingsRequestMessage request;
  PB::Settings::SettingsRequestMessage::Request* payload = request.add_payload();
  payload->set_plugin_id(333);

  PB::Settings::SettingsRequestMessage::Request::Query* item = payload->mutable_query();
  item->mutable_node()->set_path("/settings/parent");
  item->set_recursive(true);

  EXPECT_TRUE(request.payload(0).query().recursive());
}

TEST(SettingsRequestMessageTest, CreateKeysQueryRequest) {
  PB::Settings::SettingsRequestMessage request;
  PB::Settings::SettingsRequestMessage::Request* payload = request.add_payload();
  payload->set_plugin_id(444);

  PB::Settings::SettingsRequestMessage::Request::Query* item = payload->mutable_query();
  item->mutable_node()->set_path("/settings/mypath");
  item->set_recursive(false);
  item->set_include_keys(true);

  EXPECT_TRUE(request.payload(0).query().include_keys());
}

TEST(SettingsRequestMessageTest, CreateUpdateRequest) {
  PB::Settings::SettingsRequestMessage request;
  PB::Settings::SettingsRequestMessage::Request* payload = request.add_payload();
  payload->set_plugin_id(555);

  PB::Settings::SettingsRequestMessage::Request::Update* item = payload->mutable_update();
  item->mutable_node()->set_key("mykey");
  item->mutable_node()->set_path("/settings/mypath");
  item->mutable_node()->set_value("myvalue");

  EXPECT_TRUE(request.payload(0).has_update());
  EXPECT_EQ("mykey", request.payload(0).update().node().key());
  EXPECT_EQ("/settings/mypath", request.payload(0).update().node().path());
  EXPECT_EQ("myvalue", request.payload(0).update().node().value());
}

TEST(SettingsRequestMessageTest, CreateControlSaveRequest) {
  PB::Settings::SettingsRequestMessage request;
  PB::Settings::SettingsRequestMessage::Request* payload = request.add_payload();
  payload->set_plugin_id(666);

  PB::Settings::SettingsRequestMessage::Request::Control* item = payload->mutable_control();
  item->set_command(PB::Settings::Command::SAVE);
  item->set_context("my_context");

  EXPECT_TRUE(request.payload(0).has_control());
  EXPECT_EQ(PB::Settings::Command::SAVE, request.payload(0).control().command());
  EXPECT_EQ("my_context", request.payload(0).control().context());
}

TEST(SettingsRequestMessageTest, CreateRemoveKeyRequest) {
  PB::Settings::SettingsRequestMessage request;
  PB::Settings::SettingsRequestMessage::Request* payload = request.add_payload();
  payload->set_plugin_id(777);

  PB::Settings::SettingsRequestMessage::Request::Update* item = payload->mutable_update();
  item->mutable_node()->set_key("key_to_remove");
  item->mutable_node()->set_path("/settings/path");
  // Note: no value set means remove

  EXPECT_EQ("key_to_remove", request.payload(0).update().node().key());
  EXPECT_EQ("", request.payload(0).update().node().value());
}

TEST(SettingsRequestMessageTest, CreateRemovePathRequest) {
  PB::Settings::SettingsRequestMessage request;
  PB::Settings::SettingsRequestMessage::Request* payload = request.add_payload();
  payload->set_plugin_id(888);

  PB::Settings::SettingsRequestMessage::Request::Update* item = payload->mutable_update();
  item->mutable_node()->set_path("/settings/path_to_remove");

  EXPECT_EQ("/settings/path_to_remove", request.payload(0).update().node().path());
  EXPECT_EQ("", request.payload(0).update().node().key());
}

// ============================================================================
// Response message tests
// ============================================================================

TEST(SettingsResponseMessageTest, ParseSuccessResponse) {
  PB::Settings::SettingsResponseMessage response;
  auto* payload = response.add_payload();
  payload->mutable_result()->set_code(PB::Common::Result::STATUS_OK);

  EXPECT_EQ(1, response.payload_size());
  EXPECT_EQ(PB::Common::Result::STATUS_OK, response.payload(0).result().code());
}

TEST(SettingsResponseMessageTest, ParseErrorResponse) {
  PB::Settings::SettingsResponseMessage response;
  auto* payload = response.add_payload();
  payload->mutable_result()->set_code(PB::Common::Result::STATUS_ERROR);
  payload->mutable_result()->set_message("Something went wrong");

  EXPECT_EQ(PB::Common::Result::STATUS_ERROR, response.payload(0).result().code());
  EXPECT_EQ("Something went wrong", response.payload(0).result().message());
}

TEST(SettingsResponseMessageTest, ParseQueryResponse) {
  PB::Settings::SettingsResponseMessage response;
  auto* payload = response.add_payload();
  payload->mutable_result()->set_code(PB::Common::Result::STATUS_OK);
  auto* query = payload->mutable_query();
  query->mutable_node()->set_value("returned_value");

  EXPECT_TRUE(response.payload(0).has_query());
  EXPECT_EQ("returned_value", response.payload(0).query().node().value());
}

TEST(SettingsResponseMessageTest, ParseSectionsResponse) {
  PB::Settings::SettingsResponseMessage response;
  auto* payload = response.add_payload();
  payload->mutable_result()->set_code(PB::Common::Result::STATUS_OK);
  auto* query = payload->mutable_query();

  auto* node1 = query->add_nodes();
  node1->set_path("/settings/parent/section1");
  auto* node2 = query->add_nodes();
  node2->set_path("/settings/parent/section2");
  auto* node3 = query->add_nodes();
  node3->set_path("/settings/parent/section3");

  EXPECT_EQ(3, response.payload(0).query().nodes_size());
  EXPECT_EQ("/settings/parent/section1", response.payload(0).query().nodes(0).path());
  EXPECT_EQ("/settings/parent/section2", response.payload(0).query().nodes(1).path());
  EXPECT_EQ("/settings/parent/section3", response.payload(0).query().nodes(2).path());
}

TEST(SettingsResponseMessageTest, ParseKeysResponse) {
  PB::Settings::SettingsResponseMessage response;
  auto* payload = response.add_payload();
  payload->mutable_result()->set_code(PB::Common::Result::STATUS_OK);
  auto* query = payload->mutable_query();

  auto* node1 = query->add_nodes();
  node1->set_key("key1");
  auto* node2 = query->add_nodes();
  node2->set_key("key2");

  EXPECT_EQ(2, response.payload(0).query().nodes_size());
  EXPECT_EQ("key1", response.payload(0).query().nodes(0).key());
  EXPECT_EQ("key2", response.payload(0).query().nodes(1).key());
}

// ============================================================================
// Serialization roundtrip tests
// ============================================================================

TEST(SettingsSerializationTest, RequestRoundtrip) {
  PB::Settings::SettingsRequestMessage original;
  auto* payload = original.add_payload();
  payload->set_plugin_id(12345);
  auto* query = payload->mutable_query();
  query->mutable_node()->set_path("/test/path");
  query->mutable_node()->set_key("test_key");
  query->set_default_value("default_val");

  std::string serialized = original.SerializeAsString();

  PB::Settings::SettingsRequestMessage parsed;
  ASSERT_TRUE(parsed.ParseFromString(serialized));

  EXPECT_EQ(1, parsed.payload_size());
  EXPECT_EQ(12345, parsed.payload(0).plugin_id());
  EXPECT_EQ("/test/path", parsed.payload(0).query().node().path());
  EXPECT_EQ("test_key", parsed.payload(0).query().node().key());
  EXPECT_EQ("default_val", parsed.payload(0).query().default_value());
}

TEST(SettingsSerializationTest, ResponseRoundtrip) {
  PB::Settings::SettingsResponseMessage original;
  auto* payload = original.add_payload();
  payload->mutable_result()->set_code(PB::Common::Result::STATUS_OK);
  payload->mutable_query()->mutable_node()->set_value("test_value");

  std::string serialized = original.SerializeAsString();

  PB::Settings::SettingsResponseMessage parsed;
  ASSERT_TRUE(parsed.ParseFromString(serialized));

  EXPECT_EQ(PB::Common::Result::STATUS_OK, parsed.payload(0).result().code());
  EXPECT_EQ("test_value", parsed.payload(0).query().node().value());
}

TEST(SettingsSerializationTest, RegistrationRoundtrip) {
  PB::Settings::SettingsRequestMessage original;
  auto* payload = original.add_payload();
  payload->set_plugin_id(999);
  auto* reg = payload->mutable_registration();
  reg->mutable_node()->set_path("/my/path");
  reg->mutable_info()->set_title("My Title");
  reg->mutable_info()->set_description("My Description");
  reg->mutable_info()->set_advanced(true);
  reg->mutable_info()->set_sample(false);
  reg->mutable_info()->set_is_sensitive(true);

  std::string serialized = original.SerializeAsString();

  PB::Settings::SettingsRequestMessage parsed;
  ASSERT_TRUE(parsed.ParseFromString(serialized));

  EXPECT_EQ(999, parsed.payload(0).plugin_id());
  EXPECT_EQ("/my/path", parsed.payload(0).registration().node().path());
  EXPECT_EQ("My Title", parsed.payload(0).registration().info().title());
  EXPECT_EQ("My Description", parsed.payload(0).registration().info().description());
  EXPECT_TRUE(parsed.payload(0).registration().info().advanced());
  EXPECT_FALSE(parsed.payload(0).registration().info().sample());
  EXPECT_TRUE(parsed.payload(0).registration().info().is_sensitive());
}

// ============================================================================
// Edge case tests
// ============================================================================

TEST(SettingsEdgeCaseTest, EmptyPath) {
  PB::Settings::SettingsRequestMessage request;
  auto* payload = request.add_payload();
  payload->mutable_query()->mutable_node()->set_path("");
  payload->mutable_query()->mutable_node()->set_key("key");

  EXPECT_EQ("", request.payload(0).query().node().path());
}

TEST(SettingsEdgeCaseTest, EmptyKey) {
  PB::Settings::SettingsRequestMessage request;
  auto* payload = request.add_payload();
  payload->mutable_query()->mutable_node()->set_path("/path");
  payload->mutable_query()->mutable_node()->set_key("");

  EXPECT_EQ("", request.payload(0).query().node().key());
}

TEST(SettingsEdgeCaseTest, SpecialCharactersInPath) {
  PB::Settings::SettingsRequestMessage request;
  auto* payload = request.add_payload();
  payload->mutable_query()->mutable_node()->set_path("/path/with spaces/and-special_chars!@#$%");

  std::string serialized = request.SerializeAsString();
  PB::Settings::SettingsRequestMessage parsed;
  ASSERT_TRUE(parsed.ParseFromString(serialized));

  EXPECT_EQ("/path/with spaces/and-special_chars!@#$%", parsed.payload(0).query().node().path());
}

TEST(SettingsEdgeCaseTest, UnicodeInValue) {
  PB::Settings::SettingsRequestMessage request;
  auto* payload = request.add_payload();
  payload->mutable_update()->mutable_node()->set_value("Unicode: äöü ñ 日本語");

  std::string serialized = request.SerializeAsString();
  PB::Settings::SettingsRequestMessage parsed;
  ASSERT_TRUE(parsed.ParseFromString(serialized));

  EXPECT_EQ("Unicode: äöü ñ 日本語", parsed.payload(0).update().node().value());
}

TEST(SettingsEdgeCaseTest, MultiplePayloads) {
  PB::Settings::SettingsRequestMessage request;

  auto* payload1 = request.add_payload();
  payload1->set_plugin_id(1);
  payload1->mutable_query()->mutable_node()->set_path("/path1");

  auto* payload2 = request.add_payload();
  payload2->set_plugin_id(2);
  payload2->mutable_query()->mutable_node()->set_path("/path2");

  auto* payload3 = request.add_payload();
  payload3->set_plugin_id(3);
  payload3->mutable_query()->mutable_node()->set_path("/path3");

  EXPECT_EQ(3, request.payload_size());
  EXPECT_EQ(1, request.payload(0).plugin_id());
  EXPECT_EQ(2, request.payload(1).plugin_id());
  EXPECT_EQ(3, request.payload(2).plugin_id());
}

TEST(SettingsEdgeCaseTest, EmptyResponse) {
  PB::Settings::SettingsResponseMessage response;

  EXPECT_EQ(0, response.payload_size());
}

TEST(SettingsEdgeCaseTest, ResponseWithNoQuery) {
  PB::Settings::SettingsResponseMessage response;
  auto* payload = response.add_payload();
  payload->mutable_result()->set_code(PB::Common::Result::STATUS_OK);

  EXPECT_FALSE(response.payload(0).has_query());
}

// ============================================================================
// Integer value conversion tests (used by get_int/set_int)
// ============================================================================

TEST(SettingsIntConversionTest, IntToStringAndBack) {
  int original = 12345;
  std::string str_value = settings::settings_value::from_int(original);
  int parsed = settings::settings_value::to_int(str_value);

  EXPECT_EQ(original, parsed);
}

TEST(SettingsIntConversionTest, NegativeIntRoundtrip) {
  int original = -9876;
  std::string str_value = settings::settings_value::from_int(original);
  int parsed = settings::settings_value::to_int(str_value);

  EXPECT_EQ(original, parsed);
}

TEST(SettingsIntConversionTest, MaxIntRoundtrip) {
  int original = INT_MAX;
  std::string str_value = settings::settings_value::from_int(original);
  int parsed = settings::settings_value::to_int(str_value);

  EXPECT_EQ(original, parsed);
}

TEST(SettingsIntConversionTest, MinIntRoundtrip) {
  int original = INT_MIN;
  std::string str_value = settings::settings_value::from_int(original);
  int parsed = settings::settings_value::to_int(str_value);

  EXPECT_EQ(original, parsed);
}

// ============================================================================
// Boolean value conversion tests (used by get_bool/set_bool)
// ============================================================================

TEST(SettingsBoolConversionTest, TrueRoundtrip) {
  bool original = true;
  std::string str_value = settings::settings_value::from_bool(original);
  bool parsed = settings::settings_value::to_bool(str_value);

  EXPECT_EQ(original, parsed);
}

TEST(SettingsBoolConversionTest, FalseRoundtrip) {
  bool original = false;
  std::string str_value = settings::settings_value::from_bool(original);
  bool parsed = settings::settings_value::to_bool(str_value);

  EXPECT_EQ(original, parsed);
}

}  // namespace nscapi

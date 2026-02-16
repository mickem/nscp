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

#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/functions_convert.hpp>

// Message conversion tests
TEST(MessageConversionTest, make_query_from_exec) {
  PB::Commands::ExecuteResponseMessage exec_msg;
  auto* payload = exec_msg.add_payload();
  payload->set_command("test_command");
  payload->set_message("Test result");
  payload->set_result(PB::Common::ResultCode::OK);

  std::string data = exec_msg.SerializeAsString();
  nscapi::protobuf::functions::make_query_from_exec(data);

  PB::Commands::QueryResponseMessage query_msg;
  ASSERT_TRUE(query_msg.ParseFromString(data));
  EXPECT_EQ(1, query_msg.payload_size());
  EXPECT_EQ("test_command", query_msg.payload(0).command());
  EXPECT_EQ(PB::Common::ResultCode::OK, query_msg.payload(0).result());
}

TEST(MessageConversionTest, make_query_from_submit) {
  PB::Commands::SubmitResponseMessage submit_msg;
  auto* payload = submit_msg.add_payload();
  payload->set_command("test_command");
  payload->mutable_result()->set_message("Test result");
  payload->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);

  std::string data = submit_msg.SerializeAsString();
  nscapi::protobuf::functions::make_query_from_submit(data);

  PB::Commands::QueryResponseMessage query_msg;
  ASSERT_TRUE(query_msg.ParseFromString(data));
  EXPECT_EQ(1, query_msg.payload_size());
  EXPECT_EQ("test_command", query_msg.payload(0).command());
  EXPECT_EQ(1, query_msg.payload(0).lines_size());
  EXPECT_EQ("Test result", query_msg.payload(0).lines(0).message());
  EXPECT_EQ(PB::Common::ResultCode::OK, query_msg.payload(0).result());
}

TEST(MessageConversionTest, make_exec_from_submit) {
  PB::Commands::SubmitResponseMessage submit_msg;
  auto* payload = submit_msg.add_payload();
  payload->set_command("test_command");
  payload->mutable_result()->set_message("Test result");
  payload->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);

  std::string data = submit_msg.SerializeAsString();
  nscapi::protobuf::functions::make_exec_from_submit(data);

  PB::Commands::ExecuteResponseMessage exec_msg;
  ASSERT_TRUE(exec_msg.ParseFromString(data));
  EXPECT_EQ(1, exec_msg.payload_size());
  EXPECT_EQ("test_command", exec_msg.payload(0).command());
  EXPECT_EQ("Test result", exec_msg.payload(0).message());
  EXPECT_EQ(PB::Common::ResultCode::OK, exec_msg.payload(0).result());
}

TEST(MessageConversionTest, make_submit_from_query) {
  PB::Commands::QueryResponseMessage query_msg;
  auto* payload = query_msg.add_payload();
  payload->set_command("test_command");
  payload->set_result(PB::Common::ResultCode::OK);

  std::string data = query_msg.SerializeAsString();
  nscapi::protobuf::functions::make_submit_from_query(data, "test_channel", "alias", "target", "source");

  PB::Commands::SubmitRequestMessage submit_msg;
  ASSERT_TRUE(submit_msg.ParseFromString(data));
  EXPECT_EQ("test_channel", submit_msg.channel());
  EXPECT_EQ(1, submit_msg.payload_size());
  EXPECT_EQ("alias", submit_msg.payload(0).alias());
  EXPECT_EQ("test_command", submit_msg.payload(0).command());
}

// Header manipulation tests
TEST(HeaderTest, make_return_header) {
  PB::Common::Header source;
  source.set_recipient_id("recipient");
  source.set_sender_id("sender");

  PB::Common::Header target;
  nscapi::protobuf::functions::make_return_header(&target, source);

  EXPECT_EQ("recipient", target.source_id());
}

// Edge case tests
TEST(MessageConversionTest, make_submit_from_query_empty_optional_params) {
  PB::Commands::QueryResponseMessage query_msg;
  auto* payload = query_msg.add_payload();
  payload->set_command("test_command");
  payload->set_result(PB::Common::ResultCode::OK);

  std::string data = query_msg.SerializeAsString();
  // Empty alias, target, source
  nscapi::protobuf::functions::make_submit_from_query(data, "channel", "", "", "");

  PB::Commands::SubmitRequestMessage submit_msg;
  ASSERT_TRUE(submit_msg.ParseFromString(data));
  EXPECT_EQ("channel", submit_msg.channel());
  EXPECT_EQ(1, submit_msg.payload_size());
  EXPECT_EQ("", submit_msg.payload(0).alias());  // Should be empty when not provided
}

TEST(MessageConversionTest, make_submit_from_query_multiple_payloads) {
  PB::Commands::QueryResponseMessage query_msg;
  auto* payload1 = query_msg.add_payload();
  payload1->set_command("cmd1");
  payload1->set_result(PB::Common::ResultCode::OK);
  auto* payload2 = query_msg.add_payload();
  payload2->set_command("cmd2");
  payload2->set_result(PB::Common::ResultCode::WARNING);

  std::string data = query_msg.SerializeAsString();
  nscapi::protobuf::functions::make_submit_from_query(data, "channel", "alias", "", "");

  PB::Commands::SubmitRequestMessage submit_msg;
  ASSERT_TRUE(submit_msg.ParseFromString(data));
  EXPECT_EQ(2, submit_msg.payload_size());
  EXPECT_EQ("alias", submit_msg.payload(0).alias());
  EXPECT_EQ("alias", submit_msg.payload(1).alias());
}

TEST(MessageConversionTest, make_query_from_exec_multiple_payloads) {
  PB::Commands::ExecuteResponseMessage exec_msg;
  auto* payload1 = exec_msg.add_payload();
  payload1->set_command("cmd1");
  payload1->set_message("Result 1");
  payload1->set_result(PB::Common::ResultCode::OK);
  auto* payload2 = exec_msg.add_payload();
  payload2->set_command("cmd2");
  payload2->set_message("Result 2");
  payload2->set_result(PB::Common::ResultCode::WARNING);

  std::string data = exec_msg.SerializeAsString();
  nscapi::protobuf::functions::make_query_from_exec(data);

  PB::Commands::QueryResponseMessage query_msg;
  ASSERT_TRUE(query_msg.ParseFromString(data));
  EXPECT_EQ(2, query_msg.payload_size());
  EXPECT_EQ("cmd1", query_msg.payload(0).command());
  EXPECT_EQ("cmd2", query_msg.payload(1).command());
}

TEST(MessageConversionTest, make_query_from_submit_multiple_payloads) {
  PB::Commands::SubmitResponseMessage submit_msg;
  auto* payload1 = submit_msg.add_payload();
  payload1->set_command("cmd1");
  payload1->mutable_result()->set_message("Result 1");
  payload1->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
  auto* payload2 = submit_msg.add_payload();
  payload2->set_command("cmd2");
  payload2->mutable_result()->set_message("Result 2");
  payload2->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_ERROR);

  std::string data = submit_msg.SerializeAsString();
  nscapi::protobuf::functions::make_query_from_submit(data);

  PB::Commands::QueryResponseMessage query_msg;
  ASSERT_TRUE(query_msg.ParseFromString(data));
  EXPECT_EQ(2, query_msg.payload_size());
}

TEST(MessageConversionTest, make_exec_from_submit_multiple_payloads) {
  PB::Commands::SubmitResponseMessage submit_msg;
  auto* payload1 = submit_msg.add_payload();
  payload1->set_command("cmd1");
  payload1->mutable_result()->set_message("Result 1");
  payload1->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
  auto* payload2 = submit_msg.add_payload();
  payload2->set_command("cmd2");
  payload2->mutable_result()->set_message("Result 2");
  payload2->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_ERROR);

  std::string data = submit_msg.SerializeAsString();
  nscapi::protobuf::functions::make_exec_from_submit(data);

  PB::Commands::ExecuteResponseMessage exec_msg;
  ASSERT_TRUE(exec_msg.ParseFromString(data));
  EXPECT_EQ(2, exec_msg.payload_size());
  EXPECT_EQ("cmd1", exec_msg.payload(0).command());
  EXPECT_EQ("cmd2", exec_msg.payload(1).command());
}

TEST(MessageConversionTest, make_query_from_exec_empty_message) {
  PB::Commands::ExecuteResponseMessage exec_msg;
  // No payloads

  std::string data = exec_msg.SerializeAsString();
  nscapi::protobuf::functions::make_query_from_exec(data);

  PB::Commands::QueryResponseMessage query_msg;
  ASSERT_TRUE(query_msg.ParseFromString(data));
  EXPECT_EQ(0, query_msg.payload_size());
}

TEST(MessageConversionTest, make_submit_from_query_with_target) {
  PB::Commands::QueryResponseMessage query_msg;
  query_msg.mutable_header()->set_recipient_id("original_recipient");
  auto* payload = query_msg.add_payload();
  payload->set_command("cmd");

  std::string data = query_msg.SerializeAsString();
  nscapi::protobuf::functions::make_submit_from_query(data, "channel", "", "new_target", "");

  PB::Commands::SubmitRequestMessage submit_msg;
  ASSERT_TRUE(submit_msg.ParseFromString(data));
  EXPECT_EQ("new_target", submit_msg.header().recipient_id());
}

TEST(MessageConversionTest, make_submit_from_query_with_source) {
  PB::Commands::QueryResponseMessage query_msg;
  auto* payload = query_msg.add_payload();
  payload->set_command("cmd");

  std::string data = query_msg.SerializeAsString();
  nscapi::protobuf::functions::make_submit_from_query(data, "channel", "", "", "my_source");

  PB::Commands::SubmitRequestMessage submit_msg;
  ASSERT_TRUE(submit_msg.ParseFromString(data));
  EXPECT_EQ("my_source", submit_msg.header().sender_id());
}

TEST(HeaderTest, make_return_header_preserves_other_fields) {
  PB::Common::Header source;
  source.set_recipient_id("recipient");
  source.set_sender_id("sender");
  source.set_command("original_command");

  PB::Common::Header target;
  nscapi::protobuf::functions::make_return_header(&target, source);

  EXPECT_EQ("recipient", target.source_id());
  EXPECT_EQ("sender", target.sender_id());
  EXPECT_EQ("original_command", target.command());
}

TEST(HeaderTest, make_return_header_empty_recipient) {
  PB::Common::Header source;
  source.set_sender_id("sender");
  // recipient_id not set

  PB::Common::Header target;
  nscapi::protobuf::functions::make_return_header(&target, source);

  EXPECT_EQ("", target.source_id());
}

// Test result code conversions in make_query_from_submit
TEST(MessageConversionTest, make_query_from_submit_ok_status) {
  PB::Commands::SubmitResponseMessage submit_msg;
  auto* payload = submit_msg.add_payload();
  payload->set_command("cmd");
  payload->mutable_result()->set_message("OK");
  payload->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);

  std::string data = submit_msg.SerializeAsString();
  nscapi::protobuf::functions::make_query_from_submit(data);

  PB::Commands::QueryResponseMessage query_msg;
  ASSERT_TRUE(query_msg.ParseFromString(data));
  EXPECT_EQ(PB::Common::ResultCode::OK, query_msg.payload(0).result());
}

TEST(MessageConversionTest, make_query_from_submit_error_status) {
  PB::Commands::SubmitResponseMessage submit_msg;
  auto* payload = submit_msg.add_payload();
  payload->set_command("cmd");
  payload->mutable_result()->set_message("Error");
  payload->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_ERROR);

  std::string data = submit_msg.SerializeAsString();
  nscapi::protobuf::functions::make_query_from_submit(data);

  PB::Commands::QueryResponseMessage query_msg;
  ASSERT_TRUE(query_msg.ParseFromString(data));
  EXPECT_NE(PB::Common::ResultCode::OK, query_msg.payload(0).result());
}

// Test result code conversions in make_exec_from_submit
TEST(MessageConversionTest, make_exec_from_submit_ok_status) {
  PB::Commands::SubmitResponseMessage submit_msg;
  auto* payload = submit_msg.add_payload();
  payload->set_command("cmd");
  payload->mutable_result()->set_message("OK");
  payload->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);

  std::string data = submit_msg.SerializeAsString();
  nscapi::protobuf::functions::make_exec_from_submit(data);

  PB::Commands::ExecuteResponseMessage exec_msg;
  ASSERT_TRUE(exec_msg.ParseFromString(data));
  EXPECT_EQ(PB::Common::ResultCode::OK, exec_msg.payload(0).result());
  EXPECT_EQ("OK", exec_msg.payload(0).message());
}

TEST(MessageConversionTest, make_exec_from_submit_error_status) {
  PB::Commands::SubmitResponseMessage submit_msg;
  auto* payload = submit_msg.add_payload();
  payload->set_command("cmd");
  payload->mutable_result()->set_message("Error");
  payload->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_ERROR);

  std::string data = submit_msg.SerializeAsString();
  nscapi::protobuf::functions::make_exec_from_submit(data);

  PB::Commands::ExecuteResponseMessage exec_msg;
  ASSERT_TRUE(exec_msg.ParseFromString(data));
  EXPECT_NE(PB::Common::ResultCode::OK, exec_msg.payload(0).result());
}

// Test make_query_from_exec result preservation
TEST(MessageConversionTest, make_query_from_exec_warning_result) {
  PB::Commands::ExecuteResponseMessage exec_msg;
  auto* payload = exec_msg.add_payload();
  payload->set_command("cmd");
  payload->set_message("Warning");
  payload->set_result(PB::Common::ResultCode::WARNING);

  std::string data = exec_msg.SerializeAsString();
  nscapi::protobuf::functions::make_query_from_exec(data);

  PB::Commands::QueryResponseMessage query_msg;
  ASSERT_TRUE(query_msg.ParseFromString(data));
  EXPECT_EQ(PB::Common::ResultCode::WARNING, query_msg.payload(0).result());
}

TEST(MessageConversionTest, make_query_from_exec_critical_result) {
  PB::Commands::ExecuteResponseMessage exec_msg;
  auto* payload = exec_msg.add_payload();
  payload->set_command("cmd");
  payload->set_message("Critical");
  payload->set_result(PB::Common::ResultCode::CRITICAL);

  std::string data = exec_msg.SerializeAsString();
  nscapi::protobuf::functions::make_query_from_exec(data);

  PB::Commands::QueryResponseMessage query_msg;
  ASSERT_TRUE(query_msg.ParseFromString(data));
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, query_msg.payload(0).result());
}

// Test empty payload scenarios
TEST(MessageConversionTest, make_query_from_submit_empty) {
  PB::Commands::SubmitResponseMessage submit_msg;
  // No payloads

  std::string data = submit_msg.SerializeAsString();
  nscapi::protobuf::functions::make_query_from_submit(data);

  PB::Commands::QueryResponseMessage query_msg;
  ASSERT_TRUE(query_msg.ParseFromString(data));
  EXPECT_EQ(0, query_msg.payload_size());
}

TEST(MessageConversionTest, make_exec_from_submit_empty) {
  PB::Commands::SubmitResponseMessage submit_msg;
  // No payloads

  std::string data = submit_msg.SerializeAsString();
  nscapi::protobuf::functions::make_exec_from_submit(data);

  PB::Commands::ExecuteResponseMessage exec_msg;
  ASSERT_TRUE(exec_msg.ParseFromString(data));
  EXPECT_EQ(0, exec_msg.payload_size());
}

TEST(MessageConversionTest, make_submit_from_query_empty) {
  PB::Commands::QueryResponseMessage query_msg;
  // No payloads

  std::string data = query_msg.SerializeAsString();
  nscapi::protobuf::functions::make_submit_from_query(data, "channel", "", "", "");

  PB::Commands::SubmitRequestMessage submit_msg;
  ASSERT_TRUE(submit_msg.ParseFromString(data));
  EXPECT_EQ("channel", submit_msg.channel());
  EXPECT_EQ(0, submit_msg.payload_size());
}

// Test header copying
TEST(MessageConversionTest, make_query_from_exec_copies_header) {
  PB::Commands::ExecuteResponseMessage exec_msg;
  exec_msg.mutable_header()->set_sender_id("sender");
  exec_msg.mutable_header()->set_recipient_id("recipient");
  auto* payload = exec_msg.add_payload();
  payload->set_command("cmd");

  std::string data = exec_msg.SerializeAsString();
  nscapi::protobuf::functions::make_query_from_exec(data);

  PB::Commands::QueryResponseMessage query_msg;
  ASSERT_TRUE(query_msg.ParseFromString(data));
  EXPECT_EQ("sender", query_msg.header().sender_id());
  EXPECT_EQ("recipient", query_msg.header().recipient_id());
}

TEST(MessageConversionTest, make_query_from_submit_copies_header) {
  PB::Commands::SubmitResponseMessage submit_msg;
  submit_msg.mutable_header()->set_sender_id("sender");
  submit_msg.mutable_header()->set_recipient_id("recipient");
  auto* payload = submit_msg.add_payload();
  payload->set_command("cmd");

  std::string data = submit_msg.SerializeAsString();
  nscapi::protobuf::functions::make_query_from_submit(data);

  PB::Commands::QueryResponseMessage query_msg;
  ASSERT_TRUE(query_msg.ParseFromString(data));
  EXPECT_EQ("sender", query_msg.header().sender_id());
  EXPECT_EQ("recipient", query_msg.header().recipient_id());
}

TEST(MessageConversionTest, make_exec_from_submit_copies_header) {
  PB::Commands::SubmitResponseMessage submit_msg;
  submit_msg.mutable_header()->set_sender_id("sender");
  submit_msg.mutable_header()->set_recipient_id("recipient");
  auto* payload = submit_msg.add_payload();
  payload->set_command("cmd");

  std::string data = submit_msg.SerializeAsString();
  nscapi::protobuf::functions::make_exec_from_submit(data);

  PB::Commands::ExecuteResponseMessage exec_msg;
  ASSERT_TRUE(exec_msg.ParseFromString(data));
  EXPECT_EQ("sender", exec_msg.header().sender_id());
  EXPECT_EQ("recipient", exec_msg.header().recipient_id());
}

// Tests for handling of malformed/invalid input
TEST(MessageConversionTest, make_query_from_exec_invalid_input) {
  std::string data = "not valid protobuf";
  // Should not crash, even with invalid input
  nscapi::protobuf::functions::make_query_from_exec(data);
  // Result should be parseable (may be empty message)
  PB::Commands::QueryResponseMessage query_msg;
  EXPECT_TRUE(query_msg.ParseFromString(data));
}

TEST(MessageConversionTest, make_query_from_submit_invalid_input) {
  std::string data = "not valid protobuf";
  nscapi::protobuf::functions::make_query_from_submit(data);
  PB::Commands::QueryResponseMessage query_msg;
  EXPECT_TRUE(query_msg.ParseFromString(data));
}

TEST(MessageConversionTest, make_exec_from_submit_invalid_input) {
  std::string data = "not valid protobuf";
  nscapi::protobuf::functions::make_exec_from_submit(data);
  PB::Commands::ExecuteResponseMessage exec_msg;
  EXPECT_TRUE(exec_msg.ParseFromString(data));
}

TEST(MessageConversionTest, make_submit_from_query_invalid_input) {
  std::string data = "not valid protobuf";
  nscapi::protobuf::functions::make_submit_from_query(data, "channel", "", "", "");
  PB::Commands::SubmitRequestMessage submit_msg;
  EXPECT_TRUE(submit_msg.ParseFromString(data));
}

// Test with performance data preserved
TEST(MessageConversionTest, make_query_from_exec_preserves_message_content) {
  PB::Commands::ExecuteResponseMessage exec_msg;
  auto* payload = exec_msg.add_payload();
  payload->set_command("cmd");
  payload->set_message("Long detailed message with data");
  payload->set_result(PB::Common::ResultCode::OK);

  std::string data = exec_msg.SerializeAsString();
  nscapi::protobuf::functions::make_query_from_exec(data);

  PB::Commands::QueryResponseMessage query_msg;
  ASSERT_TRUE(query_msg.ParseFromString(data));
  EXPECT_EQ(1, query_msg.payload_size());
  EXPECT_EQ(1, query_msg.payload(0).lines_size());
  EXPECT_EQ("Long detailed message with data", query_msg.payload(0).lines(0).message());
}

// Test make_submit_from_query with host manipulation
TEST(MessageConversionTest, make_submit_from_query_clears_host_metadata) {
  PB::Commands::QueryResponseMessage query_msg;
  query_msg.mutable_header()->set_recipient_id("host1");
  auto* host = query_msg.mutable_header()->add_hosts();
  host->set_id("host1");
  host->set_address("192.168.1.1");
  auto* md = host->add_metadata();
  md->set_key("key");
  md->set_value("value");
  auto* payload = query_msg.add_payload();
  payload->set_command("cmd");

  std::string data = query_msg.SerializeAsString();
  nscapi::protobuf::functions::make_submit_from_query(data, "channel", "", "", "");

  PB::Commands::SubmitRequestMessage submit_msg;
  ASSERT_TRUE(submit_msg.ParseFromString(data));
  // Host should have address and metadata cleared
  EXPECT_EQ(1, submit_msg.header().hosts_size());
  EXPECT_EQ("", submit_msg.header().hosts(0).address());
  EXPECT_EQ(0, submit_msg.header().hosts(0).metadata_size());
}

// Test make_submit_from_query adds source host
TEST(MessageConversionTest, make_submit_from_query_adds_source_host) {
  PB::Commands::QueryResponseMessage query_msg;
  auto* payload = query_msg.add_payload();
  payload->set_command("cmd");

  std::string data = query_msg.SerializeAsString();
  nscapi::protobuf::functions::make_submit_from_query(data, "channel", "", "", "new_source");

  PB::Commands::SubmitRequestMessage submit_msg;
  ASSERT_TRUE(submit_msg.ParseFromString(data));
  EXPECT_EQ("new_source", submit_msg.header().sender_id());
  EXPECT_EQ(1, submit_msg.header().hosts_size());
  EXPECT_EQ("new_source", submit_msg.header().hosts(0).id());
  EXPECT_EQ("new_source", submit_msg.header().hosts(0).address());
}

// Test make_return_header with empty source
TEST(HeaderTest, make_return_header_with_metadata) {
  PB::Common::Header source;
  source.set_recipient_id("recipient");
  source.set_sender_id("sender");
  auto* md = source.add_metadata();
  md->set_key("test_key");
  md->set_value("test_value");

  PB::Common::Header target;
  nscapi::protobuf::functions::make_return_header(&target, source);

  EXPECT_EQ("recipient", target.source_id());
  EXPECT_EQ(1, target.metadata_size());
  EXPECT_EQ("test_key", target.metadata(0).key());
  EXPECT_EQ("test_value", target.metadata(0).value());
}

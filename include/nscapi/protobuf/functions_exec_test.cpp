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

#include <NSCAPI.h>
#include <gtest/gtest.h>

#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/functions_exec.hpp>
#include <nscapi/protobuf/functions_query.hpp>

// Execute request/response tests
TEST(ExecuteRequestTest, create_simple_exec_request_list) {
  std::string request;
  const std::list<std::string> args = {"arg1", "arg2"};

  nscapi::protobuf::functions::create_simple_exec_request("test_module", "test_command", args, request);

  PB::Commands::ExecuteRequestMessage message;
  ASSERT_TRUE(message.ParseFromString(request));
  EXPECT_EQ(1, message.payload_size());
  EXPECT_EQ("test_command", message.payload(0).command());
  EXPECT_EQ(2, message.payload(0).arguments_size());
}

TEST(ExecuteRequestTest, create_simple_exec_request_vector) {
  std::string request;
  const std::vector<std::string> args = {"arg1"};

  nscapi::protobuf::functions::create_simple_exec_request("", "test_command", args, request);

  PB::Commands::ExecuteRequestMessage message;
  ASSERT_TRUE(message.ParseFromString(request));
  EXPECT_EQ(1, message.payload_size());
  EXPECT_EQ("test_command", message.payload(0).command());
  EXPECT_EQ(1, message.payload(0).arguments_size());
  EXPECT_EQ("arg1", message.payload(0).arguments(0));
}

TEST(ExecuteResponseTest, create_simple_exec_response) {
  std::string response;
  const auto ret = nscapi::protobuf::functions::create_simple_exec_response("test_cmd", NSCAPI::query_return_codes::returnOK, "Success", response);

  EXPECT_EQ(NSCAPI::cmd_return_codes::isSuccess, ret);

  PB::Commands::ExecuteResponseMessage message;
  ASSERT_TRUE(message.ParseFromString(response));
  EXPECT_EQ(1, message.payload_size());
  EXPECT_EQ("test_cmd", message.payload(0).command());
  EXPECT_EQ("Success", message.payload(0).message());
  EXPECT_EQ(PB::Common::ResultCode::OK, message.payload(0).result());
}

TEST(ExecuteResponseTest, create_simple_exec_response_unknown) {
  std::string response;
  const auto ret = nscapi::protobuf::functions::create_simple_exec_response_unknown("test_cmd", "Error", response);

  EXPECT_EQ(NSCAPI::cmd_return_codes::isSuccess, ret);

  PB::Commands::ExecuteResponseMessage message;
  ASSERT_TRUE(message.ParseFromString(response));
  EXPECT_EQ(1, message.payload_size());
  EXPECT_EQ("test_cmd", message.payload(0).command());
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, message.payload(0).result());
}

TEST(ExecuteResponseTest, parse_simple_exec_response) {
  PB::Commands::ExecuteResponseMessage message;
  auto* payload1 = message.add_payload();
  payload1->set_message("Result 1");
  payload1->set_result(PB::Common::ResultCode::OK);
  auto* payload2 = message.add_payload();
  payload2->set_message("Result 2");
  payload2->set_result(PB::Common::ResultCode::WARNING);

  const auto response = message.SerializeAsString();
  std::list<std::string> results;
  const auto ret = nscapi::protobuf::functions::parse_simple_exec_response(response, results);

  EXPECT_EQ(NSCAPI::query_return_codes::returnWARN, ret);
  EXPECT_EQ(2, results.size());
  EXPECT_EQ("Result 1", results.front());
  results.pop_front();
  EXPECT_EQ("Result 2", results.front());
}

// Append payload tests for exec
TEST(AppendPayloadTest, append_simple_exec_response_payload) {
  PB::Commands::ExecuteResponseMessage::Response payload;
  nscapi::protobuf::functions::append_simple_exec_response_payload(&payload, "cmd", 2, "Error");

  EXPECT_EQ("cmd", payload.command());
  EXPECT_EQ("Error", payload.message());
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, payload.result());
}

TEST(AppendPayloadTest, append_simple_exec_request_payload) {
  PB::Commands::ExecuteRequestMessage::Request payload;
  const std::vector<std::string> args = {"arg1"};
  nscapi::protobuf::functions::append_simple_exec_request_payload(&payload, "cmd", args);

  EXPECT_EQ("cmd", payload.command());
  EXPECT_EQ(1, payload.arguments_size());
}

// Parse functions edge cases
TEST(ParseFunctionsTest, parse_simple_exec_response_malformed_input) {
  std::list<std::string> result;
  const auto ret = nscapi::protobuf::functions::parse_simple_exec_response("not valid protobuf data", result);

  EXPECT_EQ(NSCAPI::query_return_codes::returnUNKNOWN, ret);
  EXPECT_TRUE(result.empty());
}

TEST(ParseFunctionsTest, parse_simple_exec_response_empty_string) {
  std::list<std::string> result;
  const auto ret = nscapi::protobuf::functions::parse_simple_exec_response("", result);

  // Empty string may parse as empty message
  EXPECT_EQ(0, ret);
  EXPECT_TRUE(result.empty());
}

// Test module header metadata
TEST(ExecuteRequestTest, create_simple_exec_request_with_module_header) {
  std::string request;
  const std::list<std::string> args = {"arg1"};

  nscapi::protobuf::functions::create_simple_exec_request("my_module", "test_command", args, request);

  PB::Commands::ExecuteRequestMessage message;
  ASSERT_TRUE(message.ParseFromString(request));
  EXPECT_TRUE(message.has_header());
  EXPECT_EQ(1, message.header().metadata_size());
  EXPECT_EQ("target", message.header().metadata(0).key());
  EXPECT_EQ("my_module", message.header().metadata(0).value());
}

TEST(ExecuteRequestTest, create_simple_exec_request_without_module) {
  std::string request;
  const std::list<std::string> args = {"arg1"};

  nscapi::protobuf::functions::create_simple_exec_request("", "test_command", args, request);

  PB::Commands::ExecuteRequestMessage message;
  ASSERT_TRUE(message.ParseFromString(request));
  // No metadata when module is empty
  EXPECT_EQ(0, message.header().metadata_size());
}

TEST(ExecuteRequestTest, create_simple_exec_request_empty_args_list) {
  std::string request;
  const std::list<std::string> args;

  nscapi::protobuf::functions::create_simple_exec_request("", "cmd", args, request);

  PB::Commands::ExecuteRequestMessage message;
  ASSERT_TRUE(message.ParseFromString(request));
  EXPECT_EQ(1, message.payload_size());
  EXPECT_EQ("cmd", message.payload(0).command());
  EXPECT_EQ(0, message.payload(0).arguments_size());
}

TEST(ExecuteRequestTest, create_simple_exec_request_empty_args_vector) {
  std::string request;
  const std::vector<std::string> args;

  nscapi::protobuf::functions::create_simple_exec_request("", "cmd", args, request);

  PB::Commands::ExecuteRequestMessage message;
  ASSERT_TRUE(message.ParseFromString(request));
  EXPECT_EQ(0, message.payload(0).arguments_size());
}

TEST(ExecuteRequestTest, create_simple_exec_request_multiple_args) {
  std::string request;
  const std::vector<std::string> args = {"arg1", "arg2", "arg3", "arg4", "arg5"};

  nscapi::protobuf::functions::create_simple_exec_request("mod", "cmd", args, request);

  PB::Commands::ExecuteRequestMessage message;
  ASSERT_TRUE(message.ParseFromString(request));
  EXPECT_EQ(5, message.payload(0).arguments_size());
  EXPECT_EQ("arg1", message.payload(0).arguments(0));
  EXPECT_EQ("arg5", message.payload(0).arguments(4));
}

TEST(ExecuteResponseTest, parse_simple_exec_response_empty_payload) {
  PB::Commands::ExecuteResponseMessage message;
  // No payloads added

  const auto response = message.SerializeAsString();
  std::list<std::string> results;
  const auto ret = nscapi::protobuf::functions::parse_simple_exec_response(response, results);

  EXPECT_EQ(0, ret);
  EXPECT_TRUE(results.empty());
}

TEST(ExecuteResponseTest, parse_simple_exec_response_critical_result) {
  PB::Commands::ExecuteResponseMessage message;
  auto* payload = message.add_payload();
  payload->set_message("Critical error");
  payload->set_result(PB::Common::ResultCode::CRITICAL);

  const auto response = message.SerializeAsString();
  std::list<std::string> results;
  const auto ret = nscapi::protobuf::functions::parse_simple_exec_response(response, results);

  EXPECT_EQ(NSCAPI::query_return_codes::returnCRIT, ret);
  EXPECT_EQ(1, results.size());
  EXPECT_EQ("Critical error", results.front());
}

TEST(ExecuteResponseTest, create_simple_exec_response_warning) {
  std::string response;
  nscapi::protobuf::functions::create_simple_exec_response("cmd", NSCAPI::query_return_codes::returnWARN, "Warning", response);

  PB::Commands::ExecuteResponseMessage message;
  ASSERT_TRUE(message.ParseFromString(response));
  EXPECT_EQ(PB::Common::ResultCode::WARNING, message.payload(0).result());
}

TEST(ExecuteResponseTest, create_simple_exec_response_critical) {
  std::string response;
  nscapi::protobuf::functions::create_simple_exec_response("cmd", NSCAPI::query_return_codes::returnCRIT, "Critical", response);

  PB::Commands::ExecuteResponseMessage message;
  ASSERT_TRUE(message.ParseFromString(response));
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, message.payload(0).result());
}

TEST(AppendPayloadTest, append_simple_exec_request_payload_empty_args) {
  PB::Commands::ExecuteRequestMessage::Request payload;
  const std::vector<std::string> args;
  nscapi::protobuf::functions::append_simple_exec_request_payload(&payload, "cmd", args);

  EXPECT_EQ("cmd", payload.command());
  EXPECT_EQ(0, payload.arguments_size());
}

TEST(AppendPayloadTest, append_simple_exec_response_payload_ok) {
  PB::Commands::ExecuteResponseMessage::Response payload;
  nscapi::protobuf::functions::append_simple_exec_response_payload(&payload, "cmd", 0, "OK");

  EXPECT_EQ(PB::Common::ResultCode::OK, payload.result());
}

TEST(AppendPayloadTest, append_simple_exec_response_payload_warning) {
  PB::Commands::ExecuteResponseMessage::Response payload;
  nscapi::protobuf::functions::append_simple_exec_response_payload(&payload, "cmd", 1, "Warning");

  EXPECT_EQ(PB::Common::ResultCode::WARNING, payload.result());
}

TEST(AppendPayloadTest, append_simple_exec_response_payload_unknown) {
  PB::Commands::ExecuteResponseMessage::Response payload;
  nscapi::protobuf::functions::append_simple_exec_response_payload(&payload, "cmd", 3, "Unknown status");

  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, payload.result());
}

TEST(ExecuteResponseTest, create_simple_exec_response_unknown_message) {
  std::string response;
  const auto ret = nscapi::protobuf::functions::create_simple_exec_response_unknown("cmd", "Unknown error occurred", response);

  EXPECT_EQ(NSCAPI::cmd_return_codes::isSuccess, ret);

  PB::Commands::ExecuteResponseMessage message;
  ASSERT_TRUE(message.ParseFromString(response));
  EXPECT_EQ(1, message.payload_size());
  EXPECT_EQ("cmd", message.payload(0).command());
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, message.payload(0).result());
  EXPECT_EQ("Unknown error occurred", message.payload(0).message());
}

TEST(ExecuteResponseTest, create_simple_exec_response_empty_command) {
  std::string response;
  nscapi::protobuf::functions::create_simple_exec_response("", NSCAPI::query_return_codes::returnOK, "Success", response);

  PB::Commands::ExecuteResponseMessage message;
  ASSERT_TRUE(message.ParseFromString(response));
  EXPECT_EQ("", message.payload(0).command());
}

TEST(ExecuteResponseTest, create_simple_exec_response_empty_message) {
  std::string response;
  nscapi::protobuf::functions::create_simple_exec_response("cmd", NSCAPI::query_return_codes::returnOK, "", response);

  PB::Commands::ExecuteResponseMessage message;
  ASSERT_TRUE(message.ParseFromString(response));
  EXPECT_EQ("", message.payload(0).message());
}

TEST(ExecuteRequestTest, create_simple_exec_request_with_empty_command) {
  std::string request;
  const std::vector<std::string> args = {"arg1"};

  nscapi::protobuf::functions::create_simple_exec_request("module", "", args, request);

  PB::Commands::ExecuteRequestMessage message;
  ASSERT_TRUE(message.ParseFromString(request));
  EXPECT_EQ("", message.payload(0).command());
}

TEST(ExecuteResponseTest, parse_simple_exec_response_ok_and_warning) {
  // Tests aggregation: should return worst result (WARNING)
  PB::Commands::ExecuteResponseMessage message;
  auto* payload1 = message.add_payload();
  payload1->set_message("Result 1");
  payload1->set_result(PB::Common::ResultCode::OK);
  auto* payload2 = message.add_payload();
  payload2->set_message("Result 2");
  payload2->set_result(PB::Common::ResultCode::WARNING);

  const auto response = message.SerializeAsString();
  std::list<std::string> results;
  const auto ret = nscapi::protobuf::functions::parse_simple_exec_response(response, results);

  EXPECT_EQ(NSCAPI::query_return_codes::returnWARN, ret);
}

TEST(ExecuteResponseTest, parse_simple_exec_response_unknown_result) {
  PB::Commands::ExecuteResponseMessage message;
  auto* payload = message.add_payload();
  payload->set_message("Unknown");
  payload->set_result(PB::Common::ResultCode::UNKNOWN);

  const auto response = message.SerializeAsString();
  std::list<std::string> results;
  const auto ret = nscapi::protobuf::functions::parse_simple_exec_response(response, results);

  EXPECT_EQ(NSCAPI::query_return_codes::returnUNKNOWN, ret);
}

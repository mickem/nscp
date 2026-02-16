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
#include <nscapi/protobuf/functions_response.hpp>

// Response setting functions tests
TEST(ResponseFunctionsTest, set_response_good_query) {
  PB::Commands::QueryResponseMessage::Response response;
  nscapi::protobuf::functions::set_response_good(response, "Test message");

  EXPECT_EQ(PB::Common::ResultCode::OK, response.result());
  EXPECT_EQ(1, response.lines_size());
  EXPECT_EQ("Test message", response.lines(0).message());
}

TEST(ResponseFunctionsTest, set_response_good_execute) {
  PB::Commands::ExecuteResponseMessage::Response response;
  nscapi::protobuf::functions::set_response_good(response, "Test message");

  EXPECT_EQ(PB::Common::ResultCode::OK, response.result());
  EXPECT_EQ("Test message", response.message());
  EXPECT_EQ("unknown", response.command());
}

TEST(ResponseFunctionsTest, set_response_good_submit) {
  PB::Commands::SubmitResponseMessage::Response response;
  nscapi::protobuf::functions::set_response_good(response, "Test message");

  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_OK, response.result().code());
  EXPECT_EQ("Test message", response.result().message());
  EXPECT_EQ("unknown", response.command());
}

TEST(ResponseFunctionsTest, set_response_good_wdata_query) {
  PB::Commands::QueryResponseMessage::Response response;
  nscapi::protobuf::functions::set_response_good_wdata(response, "Data content");

  EXPECT_EQ(PB::Common::ResultCode::OK, response.result());
  EXPECT_EQ("Data content", response.data());
  EXPECT_EQ(1, response.lines_size());
  EXPECT_EQ("see data segment", response.lines(0).message());
}

TEST(ResponseFunctionsTest, set_response_good_wdata_submit) {
  PB::Commands::SubmitResponseMessage::Response response;
  nscapi::protobuf::functions::set_response_good_wdata(response, "Data content");

  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_OK, response.result().code());
  EXPECT_EQ("Data content", response.result().data());
  EXPECT_EQ("see data segment", response.result().message());
}

TEST(ResponseFunctionsTest, set_response_good_wdata_execute) {
  PB::Commands::ExecuteResponseMessage::Response response;
  nscapi::protobuf::functions::set_response_good_wdata(response, "Data content");

  EXPECT_EQ(PB::Common::ResultCode::OK, response.result());
  EXPECT_EQ("Data content", response.data());
  EXPECT_EQ("see data segment", response.message());
  EXPECT_EQ("unknown", response.command());
}

TEST(ResponseFunctionsTest, set_response_bad_query) {
  PB::Commands::QueryResponseMessage::Response response;
  nscapi::protobuf::functions::set_response_bad(response, "Error message");

  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, response.result());
  EXPECT_EQ(1, response.lines_size());
  EXPECT_EQ("Error message", response.lines(0).message());
  EXPECT_EQ("unknown", response.command());
}

TEST(ResponseFunctionsTest, set_response_bad_execute) {
  PB::Commands::ExecuteResponseMessage::Response response;
  nscapi::protobuf::functions::set_response_bad(response, "Error message");

  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, response.result());
  EXPECT_EQ("Error message", response.message());
  EXPECT_EQ("unknown", response.command());
}

TEST(ResponseFunctionsTest, set_response_bad_submit) {
  PB::Commands::SubmitResponseMessage::Response response;
  nscapi::protobuf::functions::set_response_bad(response, "Error message");

  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_ERROR, response.result().code());
  EXPECT_EQ("Error message", response.result().message());
  EXPECT_EQ("unknown", response.command());
}

// Tests for command preservation (command should not be overwritten if already set)
TEST(ResponseFunctionsTest, set_response_good_execute_preserves_command) {
  PB::Commands::ExecuteResponseMessage::Response response;
  response.set_command("my_command");
  nscapi::protobuf::functions::set_response_good(response, "Test message");

  EXPECT_EQ(PB::Common::ResultCode::OK, response.result());
  EXPECT_EQ("my_command", response.command());  // Should preserve existing command
}

TEST(ResponseFunctionsTest, set_response_good_submit_preserves_command) {
  PB::Commands::SubmitResponseMessage::Response response;
  response.set_command("my_command");
  nscapi::protobuf::functions::set_response_good(response, "Test message");

  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_OK, response.result().code());
  EXPECT_EQ("my_command", response.command());  // Should preserve existing command
}

TEST(ResponseFunctionsTest, set_response_good_wdata_execute_preserves_command) {
  PB::Commands::ExecuteResponseMessage::Response response;
  response.set_command("my_command");
  nscapi::protobuf::functions::set_response_good_wdata(response, "Data content");

  EXPECT_EQ("my_command", response.command());  // Should preserve existing command
}

TEST(ResponseFunctionsTest, set_response_bad_query_preserves_command) {
  PB::Commands::QueryResponseMessage::Response response;
  response.set_command("my_command");
  nscapi::protobuf::functions::set_response_bad(response, "Error message");

  EXPECT_EQ("my_command", response.command());  // Should preserve existing command
}

TEST(ResponseFunctionsTest, set_response_bad_execute_preserves_command) {
  PB::Commands::ExecuteResponseMessage::Response response;
  response.set_command("my_command");
  nscapi::protobuf::functions::set_response_bad(response, "Error message");

  EXPECT_EQ("my_command", response.command());  // Should preserve existing command
}

TEST(ResponseFunctionsTest, set_response_bad_submit_preserves_command) {
  PB::Commands::SubmitResponseMessage::Response response;
  response.set_command("my_command");
  nscapi::protobuf::functions::set_response_bad(response, "Error message");

  EXPECT_EQ("my_command", response.command());  // Should preserve existing command
}

// Tests for empty message handling
TEST(ResponseFunctionsTest, set_response_good_query_empty_message) {
  PB::Commands::QueryResponseMessage::Response response;
  nscapi::protobuf::functions::set_response_good(response, "");

  EXPECT_EQ(PB::Common::ResultCode::OK, response.result());
  EXPECT_EQ(1, response.lines_size());
  EXPECT_EQ("", response.lines(0).message());
}

TEST(ResponseFunctionsTest, set_response_good_execute_empty_message) {
  PB::Commands::ExecuteResponseMessage::Response response;
  nscapi::protobuf::functions::set_response_good(response, "");

  EXPECT_EQ(PB::Common::ResultCode::OK, response.result());
  EXPECT_EQ("", response.message());
}

TEST(ResponseFunctionsTest, set_response_good_submit_empty_message) {
  PB::Commands::SubmitResponseMessage::Response response;
  nscapi::protobuf::functions::set_response_good(response, "");

  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_OK, response.result().code());
  EXPECT_EQ("", response.result().message());
}

TEST(ResponseFunctionsTest, set_response_bad_query_empty_message) {
  PB::Commands::QueryResponseMessage::Response response;
  nscapi::protobuf::functions::set_response_bad(response, "");

  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, response.result());
  EXPECT_EQ(1, response.lines_size());
  EXPECT_EQ("", response.lines(0).message());
}

// Test multiple calls accumulate lines (for Query response)
TEST(ResponseFunctionsTest, set_response_good_query_multiple_calls_accumulate_lines) {
  PB::Commands::QueryResponseMessage::Response response;
  nscapi::protobuf::functions::set_response_good(response, "Line 1");
  nscapi::protobuf::functions::set_response_good(response, "Line 2");

  EXPECT_EQ(PB::Common::ResultCode::OK, response.result());
  EXPECT_EQ(2, response.lines_size());
  EXPECT_EQ("Line 1", response.lines(0).message());
  EXPECT_EQ("Line 2", response.lines(1).message());
}

TEST(ResponseFunctionsTest, set_response_bad_query_multiple_calls_accumulate_lines) {
  PB::Commands::QueryResponseMessage::Response response;
  nscapi::protobuf::functions::set_response_bad(response, "Error 1");
  nscapi::protobuf::functions::set_response_bad(response, "Error 2");

  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, response.result());
  EXPECT_EQ(2, response.lines_size());
  EXPECT_EQ("Error 1", response.lines(0).message());
  EXPECT_EQ("Error 2", response.lines(1).message());
}

// Test wdata with empty data
TEST(ResponseFunctionsTest, set_response_good_wdata_query_empty_data) {
  PB::Commands::QueryResponseMessage::Response response;
  nscapi::protobuf::functions::set_response_good_wdata(response, "");

  EXPECT_EQ(PB::Common::ResultCode::OK, response.result());
  EXPECT_EQ("", response.data());
  EXPECT_EQ("see data segment", response.lines(0).message());
}

TEST(ResponseFunctionsTest, set_response_good_wdata_execute_empty_data) {
  PB::Commands::ExecuteResponseMessage::Response response;
  nscapi::protobuf::functions::set_response_good_wdata(response, "");

  EXPECT_EQ(PB::Common::ResultCode::OK, response.result());
  EXPECT_EQ("", response.data());
  EXPECT_EQ("see data segment", response.message());
}

TEST(ResponseFunctionsTest, set_response_good_wdata_submit_empty_data) {
  PB::Commands::SubmitResponseMessage::Response response;
  nscapi::protobuf::functions::set_response_good_wdata(response, "");

  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_OK, response.result().code());
  EXPECT_EQ("", response.result().data());
  EXPECT_EQ("see data segment", response.result().message());
}

// Tests for special characters in messages
TEST(ResponseFunctionsTest, set_response_good_query_special_chars) {
  PB::Commands::QueryResponseMessage::Response response;
  nscapi::protobuf::functions::set_response_good(response, "Test with special: <>&\"'");

  EXPECT_EQ(PB::Common::ResultCode::OK, response.result());
  EXPECT_EQ("Test with special: <>&\"'", response.lines(0).message());
}

TEST(ResponseFunctionsTest, set_response_good_execute_special_chars) {
  PB::Commands::ExecuteResponseMessage::Response response;
  nscapi::protobuf::functions::set_response_good(response, "UTF-8: äöü €");

  EXPECT_EQ(PB::Common::ResultCode::OK, response.result());
  EXPECT_EQ("UTF-8: äöü €", response.message());
}

TEST(ResponseFunctionsTest, set_response_bad_execute_empty_command_not_overwritten) {
  PB::Commands::ExecuteResponseMessage::Response response;
  // Don't set command - should default to "unknown"
  nscapi::protobuf::functions::set_response_bad(response, "Error");

  EXPECT_EQ("unknown", response.command());
}

TEST(ResponseFunctionsTest, set_response_bad_submit_empty_command_not_overwritten) {
  PB::Commands::SubmitResponseMessage::Response response;
  // Don't set command - should default to "unknown"
  nscapi::protobuf::functions::set_response_bad(response, "Error");

  EXPECT_EQ("unknown", response.command());
}

// Tests for very long messages
TEST(ResponseFunctionsTest, set_response_good_query_long_message) {
  PB::Commands::QueryResponseMessage::Response response;
  std::string longMsg(10000, 'x');
  nscapi::protobuf::functions::set_response_good(response, longMsg);

  EXPECT_EQ(PB::Common::ResultCode::OK, response.result());
  EXPECT_EQ(10000, response.lines(0).message().size());
}

TEST(ResponseFunctionsTest, set_response_good_wdata_submit_preserves_existing_command) {
  PB::Commands::SubmitResponseMessage::Response response;
  response.set_command("my_command");
  nscapi::protobuf::functions::set_response_good_wdata(response, "Data");

  EXPECT_EQ("my_command", response.command());
}

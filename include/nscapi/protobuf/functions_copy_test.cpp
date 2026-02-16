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
#include <nscapi/protobuf/functions_copy.hpp>

// Response copy tests
TEST(ResponseCopyTest, copy_exec_to_query) {
  PB::Commands::ExecuteResponseMessage::Response source;
  source.set_message("Test message");
  source.set_command("test_cmd");
  source.set_result(PB::Common::ResultCode::WARNING);

  PB::Commands::QueryResponseMessage::Response target;
  nscapi::protobuf::functions::copy_response("cmd", &target, source);

  EXPECT_EQ("cmd", target.command());
  EXPECT_EQ(1, target.lines_size());
  EXPECT_EQ("Test message", target.lines(0).message());
}

TEST(ResponseCopyTest, copy_submit_to_query) {
  PB::Commands::SubmitResponseMessage::Response source;
  source.mutable_result()->set_message("Test message");
  source.mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);

  PB::Commands::QueryResponseMessage::Response target;
  nscapi::protobuf::functions::copy_response("cmd", &target, source);

  EXPECT_EQ("cmd", target.command());
  EXPECT_EQ(PB::Common::ResultCode::OK, target.result());
  EXPECT_EQ(1, target.lines_size());
  EXPECT_EQ("Test message", target.lines(0).message());
}

TEST(ResponseCopyTest, copy_query_to_exec) {
  PB::Commands::QueryResponseMessage::Response source;
  source.set_command("test_cmd");
  source.set_result(PB::Common::ResultCode::CRITICAL);
  auto* line = source.add_lines();
  line->set_message("Critical issue");

  PB::Commands::ExecuteResponseMessage::Response target;
  nscapi::protobuf::functions::copy_response("cmd", &target, source);

  EXPECT_EQ("test_cmd", target.command());
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, target.result());
  EXPECT_EQ("Critical issue", target.message());
}

TEST(ResponseCopyTest, copy_query_to_query) {
  PB::Commands::QueryResponseMessage::Response source;
  source.set_command("test_cmd");
  source.set_result(PB::Common::ResultCode::OK);
  auto* line = source.add_lines();
  line->set_message("Test message");

  PB::Commands::QueryResponseMessage::Response target;
  nscapi::protobuf::functions::copy_response("cmd", &target, source);

  EXPECT_EQ("test_cmd", target.command());
  EXPECT_EQ(PB::Common::ResultCode::OK, target.result());
  EXPECT_EQ(1, target.lines_size());
  EXPECT_EQ("Test message", target.lines(0).message());
}

TEST(ResponseCopyTest, copy_exec_to_exec) {
  PB::Commands::ExecuteResponseMessage::Response source;
  source.set_command("test_cmd");
  source.set_message("Test message");
  source.set_result(PB::Common::ResultCode::WARNING);

  PB::Commands::ExecuteResponseMessage::Response target;
  nscapi::protobuf::functions::copy_response("cmd", &target, source);

  EXPECT_EQ("test_cmd", target.command());
  EXPECT_EQ(PB::Common::ResultCode::WARNING, target.result());
  EXPECT_EQ("Test message", target.message());
}

TEST(ResponseCopyTest, copy_submit_to_exec) {
  PB::Commands::SubmitResponseMessage::Response source;
  source.set_command("test_cmd");
  source.mutable_result()->set_message("Test message");
  source.mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);

  PB::Commands::ExecuteResponseMessage::Response target;
  nscapi::protobuf::functions::copy_response("cmd", &target, source);

  EXPECT_EQ("test_cmd", target.command());
  EXPECT_EQ(PB::Common::ResultCode::OK, target.result());
  EXPECT_EQ("Test message", target.message());
}

TEST(ResponseCopyTest, copy_exec_to_submit) {
  PB::Commands::ExecuteResponseMessage::Response source;
  source.set_message("Test message");

  PB::Commands::SubmitResponseMessage::Response target;
  nscapi::protobuf::functions::copy_response("cmd", &target, source);

  EXPECT_EQ("Test message", target.result().message());
}

TEST(ResponseCopyTest, copy_submit_to_submit) {
  PB::Commands::SubmitResponseMessage::Response source;
  source.set_command("test_cmd");
  source.mutable_result()->set_message("Test message");
  source.mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);

  PB::Commands::SubmitResponseMessage::Response target;
  nscapi::protobuf::functions::copy_response("cmd", &target, source);

  EXPECT_EQ("test_cmd", target.command());
  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_OK, target.result().code());
  EXPECT_EQ("Test message", target.result().message());
}

TEST(ResponseCopyTest, copy_query_to_submit) {
  PB::Commands::QueryResponseMessage::Response source;
  source.set_command("test_cmd");
  source.set_result(PB::Common::ResultCode::WARNING);
  auto* line = source.add_lines();
  line->set_message("Warning message");

  PB::Commands::SubmitResponseMessage::Response target;
  nscapi::protobuf::functions::copy_response("cmd", &target, source);

  EXPECT_EQ("Warning message", target.result().message());
  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_ERROR, target.result().code());
}

// Edge case tests for empty/null scenarios
TEST(ResponseCopyTest, copy_exec_to_query_empty_message) {
  const PB::Commands::ExecuteResponseMessage::Response source;
  // Empty message

  PB::Commands::QueryResponseMessage::Response target;
  nscapi::protobuf::functions::copy_response("cmd", &target, source);

  EXPECT_EQ("cmd", target.command());
  EXPECT_EQ(1, target.lines_size());
  EXPECT_EQ("", target.lines(0).message());
}

TEST(ResponseCopyTest, copy_query_to_exec_multiple_lines) {
  PB::Commands::QueryResponseMessage::Response source;
  source.set_command("test_cmd");
  source.set_result(PB::Common::ResultCode::OK);
  auto* line1 = source.add_lines();
  line1->set_message("Line 1");
  auto* line2 = source.add_lines();
  line2->set_message("Line 2");

  PB::Commands::ExecuteResponseMessage::Response target;
  nscapi::protobuf::functions::copy_response("cmd", &target, source);

  // Should concatenate lines in the message
  EXPECT_EQ("test_cmd", target.command());
  EXPECT_EQ("Line 1Line 2", target.message());
  EXPECT_EQ(PB::Common::ResultCode::OK, target.result());
}

TEST(ResponseCopyTest, copy_query_to_submit_ok_result) {
  PB::Commands::QueryResponseMessage::Response source;
  source.set_result(PB::Common::ResultCode::OK);
  auto* line = source.add_lines();
  line->set_message("OK message");

  PB::Commands::SubmitResponseMessage::Response target;
  nscapi::protobuf::functions::copy_response("cmd", &target, source);

  EXPECT_EQ("OK message", target.result().message());
  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_OK, target.result().code());
}

TEST(ResponseCopyTest, copy_query_to_submit_critical_result) {
  PB::Commands::QueryResponseMessage::Response source;
  source.set_result(PB::Common::ResultCode::CRITICAL);
  auto* line = source.add_lines();
  line->set_message("Critical message");

  PB::Commands::SubmitResponseMessage::Response target;
  nscapi::protobuf::functions::copy_response("cmd", &target, source);

  EXPECT_EQ("Critical message", target.result().message());
  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_ERROR, target.result().code());
}

TEST(ResponseCopyTest, copy_submit_to_query_error_status) {
  PB::Commands::SubmitResponseMessage::Response source;
  source.mutable_result()->set_message("Error message");
  source.mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_ERROR);

  PB::Commands::QueryResponseMessage::Response target;
  nscapi::protobuf::functions::copy_response("cmd", &target, source);

  EXPECT_EQ("cmd", target.command());
  // STATUS_ERROR should map to non-OK result
  EXPECT_NE(PB::Common::ResultCode::OK, target.result());
}

TEST(ResponseCopyTest, copy_exec_to_submit_empty) {
  const PB::Commands::ExecuteResponseMessage::Response source;
  // All fields empty

  PB::Commands::SubmitResponseMessage::Response target;
  nscapi::protobuf::functions::copy_response("cmd", &target, source);

  EXPECT_EQ("", target.result().message());
}

TEST(ResponseCopyTest, copy_query_to_query_preserves_all_fields) {
  PB::Commands::QueryResponseMessage::Response source;
  source.set_command("original_cmd");
  source.set_result(PB::Common::ResultCode::WARNING);
  auto* line = source.add_lines();
  line->set_message("Test message");
  auto* perf = line->add_perf();
  perf->set_alias("perf_data");

  PB::Commands::QueryResponseMessage::Response target;
  nscapi::protobuf::functions::copy_response("cmd", &target, source);

  // CopyFrom should preserve everything
  EXPECT_EQ("original_cmd", target.command());
  EXPECT_EQ(PB::Common::ResultCode::WARNING, target.result());
  EXPECT_EQ(1, target.lines_size());
  EXPECT_EQ("Test message", target.lines(0).message());
  EXPECT_EQ(1, target.lines(0).perf_size());
  EXPECT_EQ("perf_data", target.lines(0).perf(0).alias());
}

TEST(ResponseCopyTest, copy_exec_to_exec_preserves_all_fields) {
  PB::Commands::ExecuteResponseMessage::Response source;
  source.set_command("original_cmd");
  source.set_message("Test message");
  source.set_result(PB::Common::ResultCode::CRITICAL);

  PB::Commands::ExecuteResponseMessage::Response target;
  nscapi::protobuf::functions::copy_response("cmd", &target, source);

  // CopyFrom should preserve everything
  EXPECT_EQ("original_cmd", target.command());
  EXPECT_EQ("Test message", target.message());
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, target.result());
}

TEST(ResponseCopyTest, copy_submit_to_submit_preserves_all_fields) {
  PB::Commands::SubmitResponseMessage::Response source;
  source.set_command("original_cmd");
  source.mutable_result()->set_message("Test message");
  source.mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);

  PB::Commands::SubmitResponseMessage::Response target;
  nscapi::protobuf::functions::copy_response("cmd", &target, source);

  // CopyFrom should preserve everything
  EXPECT_EQ("original_cmd", target.command());
  EXPECT_EQ("Test message", target.result().message());
  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_OK, target.result().code());
}

// Test command parameter usage in non-CopyFrom cases
TEST(ResponseCopyTest, copy_exec_to_query_uses_command_param) {
  PB::Commands::ExecuteResponseMessage::Response source;
  source.set_command("source_cmd");
  source.set_message("Test message");

  PB::Commands::QueryResponseMessage::Response target;
  nscapi::protobuf::functions::copy_response("override_cmd", &target, source);

  // This variant should use the command parameter
  EXPECT_EQ("override_cmd", target.command());
}

TEST(ResponseCopyTest, copy_submit_to_query_uses_command_param) {
  PB::Commands::SubmitResponseMessage::Response source;
  source.set_command("source_cmd");
  source.mutable_result()->set_message("Test message");

  PB::Commands::QueryResponseMessage::Response target;
  nscapi::protobuf::functions::copy_response("override_cmd", &target, source);

  // This variant should use the command parameter
  EXPECT_EQ("override_cmd", target.command());
}

// Test query to submit with WARNING result
TEST(ResponseCopyTest, copy_query_to_submit_warning_result) {
  PB::Commands::QueryResponseMessage::Response source;
  source.set_result(PB::Common::ResultCode::WARNING);
  auto* line = source.add_lines();
  line->set_message("Warning message");

  PB::Commands::SubmitResponseMessage::Response target;
  nscapi::protobuf::functions::copy_response("cmd", &target, source);

  EXPECT_EQ("Warning message", target.result().message());
  // WARNING should map to STATUS_ERROR (non-OK)
  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_ERROR, target.result().code());
}

// Test query to submit with UNKNOWN result
TEST(ResponseCopyTest, copy_query_to_submit_unknown_result) {
  PB::Commands::QueryResponseMessage::Response source;
  source.set_result(PB::Common::ResultCode::UNKNOWN);
  auto* line = source.add_lines();
  line->set_message("Unknown message");

  PB::Commands::SubmitResponseMessage::Response target;
  nscapi::protobuf::functions::copy_response("cmd", &target, source);

  EXPECT_EQ("Unknown message", target.result().message());
}

// Test query to exec uses source command not parameter
TEST(ResponseCopyTest, copy_query_to_exec_uses_source_command) {
  PB::Commands::QueryResponseMessage::Response source;
  source.set_command("source_cmd");
  source.set_result(PB::Common::ResultCode::OK);
  auto* line = source.add_lines();
  line->set_message("Test");

  PB::Commands::ExecuteResponseMessage::Response target;
  nscapi::protobuf::functions::copy_response("override_cmd", &target, source);

  // This variant should use source command, not the parameter
  EXPECT_EQ("source_cmd", target.command());
}

// Test submit to exec uses source command
TEST(ResponseCopyTest, copy_submit_to_exec_uses_source_command) {
  PB::Commands::SubmitResponseMessage::Response source;
  source.set_command("source_cmd");
  source.mutable_result()->set_message("Test");
  source.mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);

  PB::Commands::ExecuteResponseMessage::Response target;
  nscapi::protobuf::functions::copy_response("override_cmd", &target, source);

  // This variant should use source command
  EXPECT_EQ("source_cmd", target.command());
}

// Test with no lines in query source
TEST(ResponseCopyTest, copy_query_to_exec_no_lines) {
  PB::Commands::QueryResponseMessage::Response source;
  source.set_command("cmd");
  source.set_result(PB::Common::ResultCode::OK);
  // No lines added

  PB::Commands::ExecuteResponseMessage::Response target;
  nscapi::protobuf::functions::copy_response("cmd", &target, source);

  EXPECT_EQ("cmd", target.command());
  EXPECT_EQ(PB::Common::ResultCode::OK, target.result());
}

TEST(ResponseCopyTest, copy_query_to_submit_no_lines) {
  PB::Commands::QueryResponseMessage::Response source;
  source.set_result(PB::Common::ResultCode::OK);
  // No lines added

  PB::Commands::SubmitResponseMessage::Response target;
  nscapi::protobuf::functions::copy_response("cmd", &target, source);

  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_OK, target.result().code());
}

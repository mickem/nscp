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

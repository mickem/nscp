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

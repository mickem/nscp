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
#include <nscapi/protobuf/functions_submit.hpp>

// Submit request/response tests
TEST(SubmitRequestTest, create_simple_submit_request) {
  std::string buffer;
  nscapi::protobuf::functions::create_simple_submit_request("test_channel", "test_command", NSCAPI::query_return_codes::returnWARN, "Warning message",
                                                            "metric=50", buffer);

  PB::Commands::SubmitRequestMessage message;
  ASSERT_TRUE(message.ParseFromString(buffer));
  EXPECT_EQ("test_channel", message.channel());
  EXPECT_EQ(1, message.payload_size());
  EXPECT_EQ("test_command", message.payload(0).command());
  EXPECT_EQ(PB::Common::ResultCode::WARNING, message.payload(0).result());
}

TEST(SubmitResponseTest, create_simple_submit_response_ok) {
  std::string buffer;
  nscapi::protobuf::functions::create_simple_submit_response_ok("channel", "command", "OK message", buffer);

  PB::Commands::SubmitResponseMessage message;
  ASSERT_TRUE(message.ParseFromString(buffer));
  EXPECT_EQ(1, message.payload_size());
  EXPECT_EQ("command", message.payload(0).command());
  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_OK, message.payload(0).result().code());
  EXPECT_EQ("OK message", message.payload(0).result().message());
}

TEST(SubmitResponseTest, parse_simple_submit_response) {
  std::string buffer;
  nscapi::protobuf::functions::create_simple_submit_response_ok("channel", "command", "Success", buffer);

  std::string response_msg;
  const auto result = nscapi::protobuf::functions::parse_simple_submit_response(buffer, response_msg);

  EXPECT_TRUE(result);
  EXPECT_EQ("Success", response_msg);
}

// Append payload tests for submit
TEST(AppendPayloadTest, append_simple_submit_response_payload) {
  PB::Commands::SubmitResponseMessage::Response payload;
  nscapi::protobuf::functions::append_simple_submit_response_payload(&payload, "cmd", true, "OK");

  EXPECT_EQ("cmd", payload.command());
  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_OK, payload.result().code());
  EXPECT_EQ("OK", payload.result().message());
}

// Parse functions edge cases
TEST(ParseFunctionsTest, parse_simple_submit_response_malformed_input) {
  std::string response;
  const bool result = nscapi::protobuf::functions::parse_simple_submit_response("not valid protobuf data", response);

  EXPECT_FALSE(result);
  EXPECT_EQ("Failed to parse submit response message", response);
}

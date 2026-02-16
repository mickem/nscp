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

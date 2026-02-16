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

// Query request/response tests
TEST(QueryRequestTest, create_simple_query_request_list) {
  std::string buffer;
  const std::list<std::string> args = {"arg1", "arg2", "arg3"};

  nscapi::protobuf::functions::create_simple_query_request("test_command", args, buffer);

  PB::Commands::QueryRequestMessage message;
  ASSERT_TRUE(message.ParseFromString(buffer));
  EXPECT_EQ(1, message.payload_size());
  EXPECT_EQ("test_command", message.payload(0).command());
  EXPECT_EQ(3, message.payload(0).arguments_size());
  EXPECT_EQ("arg1", message.payload(0).arguments(0));
  EXPECT_EQ("arg2", message.payload(0).arguments(1));
  EXPECT_EQ("arg3", message.payload(0).arguments(2));
}

TEST(QueryRequestTest, create_simple_query_request_vector) {
  std::string buffer;
  const std::vector<std::string> args = {"arg1", "arg2"};

  nscapi::protobuf::functions::create_simple_query_request("test_command", args, buffer);

  PB::Commands::QueryRequestMessage message;
  ASSERT_TRUE(message.ParseFromString(buffer));
  EXPECT_EQ(1, message.payload_size());
  EXPECT_EQ("test_command", message.payload(0).command());
  EXPECT_EQ(2, message.payload(0).arguments_size());
}

TEST(QueryRequestTest, parse_simple_query_request) {
  std::string buffer;
  const std::vector<std::string> input_args = {"arg1", "arg2", "arg3"};
  nscapi::protobuf::functions::create_simple_query_request("test_command", input_args, buffer);

  std::list<std::string> output_args;
  nscapi::protobuf::functions::parse_simple_query_request(output_args, buffer);

  EXPECT_EQ(3, output_args.size());
  auto it = output_args.begin();
  EXPECT_EQ("arg1", *it++);
  EXPECT_EQ("arg2", *it++);
  EXPECT_EQ("arg3", *it++);
}

TEST(QueryResponseTest, parse_simple_query_response) {
  PB::Commands::QueryResponseMessage message;
  auto* payload = message.add_payload();
  payload->set_result(PB::Common::ResultCode::WARNING);
  auto* line = payload->add_lines();
  line->set_message("Test warning");

  const auto response = message.SerializeAsString();
  std::string msg, perf;
  const auto ret = nscapi::protobuf::functions::parse_simple_query_response(response, msg, perf, 0);

  EXPECT_EQ(NSCAPI::query_return_codes::returnWARN, ret);
  EXPECT_EQ("Test warning", msg);
}

TEST(QueryResponseTest, parse_simple_query_response_with_perf) {
  PB::Commands::QueryResponseMessage message;
  auto* payload = message.add_payload();
  payload->set_result(PB::Common::ResultCode::OK);
  auto* line = payload->add_lines();
  line->set_message("All OK");
  auto* perf = line->add_perf();
  perf->set_alias("test");
  perf->mutable_float_value()->set_value(42.5);
  perf->mutable_float_value()->set_unit("ms");

  const auto response = message.SerializeAsString();
  std::string msg, perf_out;
  const auto ret = nscapi::protobuf::functions::parse_simple_query_response(response, msg, perf_out, 0);

  EXPECT_EQ(NSCAPI::query_return_codes::returnOK, ret);
  EXPECT_EQ("All OK", msg);
  EXPECT_EQ("'test'=42.5ms", perf_out);
}

TEST(QueryResponseTest, query_data_to_nagios_string) {
  PB::Commands::QueryResponseMessage message;
  auto* payload = message.add_payload();
  auto* line = payload->add_lines();
  line->set_message("Test output");

  const auto result = nscapi::protobuf::functions::query_data_to_nagios_string(message, 0);
  EXPECT_EQ("Test output", result);
}

TEST(QueryResponseTest, query_data_to_nagios_string_with_perf) {
  PB::Commands::QueryResponseMessage::Response payload;
  auto* line = payload.add_lines();
  line->set_message("Test output");
  auto* perf = line->add_perf();
  perf->set_alias("metric");
  perf->mutable_float_value()->set_value(100);

  const auto result = nscapi::protobuf::functions::query_data_to_nagios_string(payload, 0);
  EXPECT_EQ("Test output|'metric'=100", result);
}

TEST(QueryResponseTest, parse_simple_query_response_empty) {
  PB::Commands::QueryResponseMessage message;

  const auto response = message.SerializeAsString();
  std::string msg, perf;
  const auto ret = nscapi::protobuf::functions::parse_simple_query_response(response, msg, perf, 0);

  EXPECT_EQ(NSCAPI::query_return_codes::returnUNKNOWN, ret);
}

TEST(QueryResponseTest, parse_simple_query_response_no_lines) {
  PB::Commands::QueryResponseMessage message;
  auto* payload = message.add_payload();
  payload->set_result(PB::Common::ResultCode::OK);

  const auto response = message.SerializeAsString();
  std::string msg, perf;
  const auto ret = nscapi::protobuf::functions::parse_simple_query_response(response, msg, perf, 0);

  EXPECT_EQ(NSCAPI::query_return_codes::returnUNKNOWN, ret);
}

TEST(QueryResponseTest, query_data_to_nagios_string_multiple_lines) {
  PB::Commands::QueryResponseMessage message;
  auto* payload = message.add_payload();
  auto* line1 = payload->add_lines();
  line1->set_message("Line 1");
  auto* line2 = payload->add_lines();
  line2->set_message("Line 2");

  const auto result = nscapi::protobuf::functions::query_data_to_nagios_string(message, 0);
  EXPECT_EQ("Line 1Line 2", result);
}

// Append payload tests for query
TEST(AppendPayloadTest, append_simple_query_response_payload) {
  PB::Commands::QueryResponseMessage::Response payload;
  nscapi::protobuf::functions::append_simple_query_response_payload(&payload, "cmd", NSCAPI::query_return_codes::returnCRIT, "Critical error", "metric=100");

  EXPECT_EQ("cmd", payload.command());
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, payload.result());
  EXPECT_EQ(1, payload.lines_size());
  EXPECT_EQ("Critical error", payload.lines(0).message());
  EXPECT_GT(payload.lines(0).perf_size(), 0);
  EXPECT_EQ("metric", payload.lines(0).perf(0).alias());
  EXPECT_EQ(100.0, payload.lines(0).perf(0).float_value().value());
  EXPECT_EQ("", payload.lines(0).perf(0).float_value().unit());
}

TEST(AppendPayloadTest, append_simple_query_request_payload) {
  PB::Commands::QueryRequestMessage::Request payload;
  const std::vector<std::string> args = {"arg1", "arg2"};
  nscapi::protobuf::functions::append_simple_query_request_payload(&payload, "cmd", args);

  EXPECT_EQ("cmd", payload.command());
  EXPECT_EQ(2, payload.arguments_size());
  EXPECT_EQ("arg1", payload.arguments(0));
  EXPECT_EQ("arg2", payload.arguments(1));
}

// Parse functions edge cases
TEST(ParseFunctionsTest, parse_simple_query_request_malformed_input) {
  std::list<std::string> args;
  // Should not throw or crash, just return with empty args
  nscapi::protobuf::functions::parse_simple_query_request(args, "not valid protobuf data");

  EXPECT_TRUE(args.empty());
}

TEST(ParseFunctionsTest, parse_simple_query_response_malformed_input) {
  std::string msg, perf;
  const auto ret = nscapi::protobuf::functions::parse_simple_query_response("not valid protobuf data", msg, perf, 0);

  EXPECT_EQ(NSCAPI::query_return_codes::returnUNKNOWN, ret);
}

TEST(ParseFunctionsTest, parse_simple_query_response_empty_string) {
  std::string msg, perf;
  const auto ret = nscapi::protobuf::functions::parse_simple_query_response("", msg, perf, 0);

  EXPECT_EQ(NSCAPI::query_return_codes::returnUNKNOWN, ret);
}

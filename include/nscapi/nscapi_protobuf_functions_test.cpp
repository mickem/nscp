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

#include <nscapi/nscapi_protobuf_command.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <str/format.hpp>
#include <string>
#include <vector>

#include "nscapi_protobuf_nagios.hpp"

std::string do_parse(const std::string& str, const std::size_t max_length = nscapi::protobuf::functions::no_truncation) {
  PB::Commands::QueryResponseMessage::Response::Line r;
  nscapi::protobuf::functions::parse_performance_data(&r, str);
  return nscapi::protobuf::functions::build_performance_data(r, max_length);
}

TEST(PerfDataTest, fractions) {
  EXPECT_EQ("'aaa'=1.23374g;0.12345;4.47538;2.23747;5.94849", do_parse("aaa=1.2337399999999999999g;0.123456;4.4753845;2.2374742;5.9484945"));
}

TEST(PerfDataTest, empty_string) { EXPECT_EQ("", do_parse("")); }

TEST(PerfDataTest, full_string) { EXPECT_EQ("'aaa'=1g;0;4;2;5", do_parse("aaa=1g;0;4;2;5")); }
TEST(PerfDataTest, full_string_with_ticks) { EXPECT_EQ("'aaa'=1g;0;4;2;5", do_parse("aaa=1g;0;4;2;5")); }
TEST(PerfDataTest, full_spaces) { EXPECT_EQ("'aaa'=1g;0;4;2;5", do_parse("     'aaa'=1g;0;4;2;5     ")); }
TEST(PerfDataTest, mltiple_strings) { EXPECT_EQ("'aaa'=1g;0;4;2;5 'bbb'=2g;3;4;2;5", do_parse("aaa=1g;0;4;2;5 bbb=2g;3;4;2;5")); }
TEST(PerfDataTest, only_value) { EXPECT_EQ("'aaa'=1g", do_parse("aaa=1g")); }
TEST(PerfDataTest, multiple_only_values) { EXPECT_EQ("'aaa'=1g 'bbb'=2g 'ccc'=3g", do_parse("aaa=1g bbb=2g 'ccc'=3g")); }
TEST(PerfDataTest, multiple_only_values_no_units) { EXPECT_EQ("'aaa'=1 'bbb'=2 'ccc'=3", do_parse("aaa=1 'bbb'=2 ccc=3")); }
TEST(PerfDataTest, value_with_warncrit) { EXPECT_EQ("'aaa'=1g;0;5", do_parse("aaa=1g;0;5")); }
TEST(PerfDataTest, value_without_warncrit_with_maxmin) { EXPECT_EQ("'aaa'=1g;;;0;5", do_parse("aaa=1g;;;0;5")); }
TEST(PerfDataTest, value_without_warncrit_maxmin) { EXPECT_EQ("'aaa'=1g", do_parse("aaa=1g")); }
TEST(PerfDataTest, leading_space) {
  EXPECT_EQ("'aaa'=1g", do_parse(" aaa=1g"));
  EXPECT_EQ("'aaa'=1g", do_parse("                   aaa=1g"));
}
TEST(PerfDataTest, leading_spaces) {
  EXPECT_EQ("'aaa'=1g", do_parse("     aaa=1g"));
  EXPECT_EQ("'aaa'=1g", do_parse("                   aaa=1g"));
}
TEST(PerfDataTest, negative_vvalues) { EXPECT_EQ("'aaa'=-1g;-0;-4;-2;-5 'bbb'=2g;-3;4;-2;5", do_parse("aaa=-1g;-0;-4;-2;-5 bbb=2g;-3;4;-2;5")); }
TEST(PerfDataTest, value_without_long_uom) { EXPECT_EQ("'aaa'=1ggggg;;;0;5", do_parse("aaa=1ggggg;;;0;5")); }
TEST(PerfDataTest, value_without____uom) { EXPECT_EQ("'aaa'=1gg__gg;;;0;5", do_parse("aaa=1gg__gg;;;0;5")); }

TEST(PerfDataTest, float_value) { EXPECT_EQ("'aaa'=0gig;;;0;5", do_parse("aaa=0.00gig;;;0;5")); }
TEST(PerfDataTest, float_value_rounding_1) { EXPECT_EQ("'aaa'=1.01g;1.02;1.03;1.04;1.05", do_parse("aaa=1.01g;1.02;1.03;1.04;1.05")); }
TEST(PerfDataTest, float_value_rounding_2) {
#if (_MSC_VER == 1700)
  EXPECT_EQ("'aaa'=1.0001g;1.02;1.03;1.04;1.05", do_parse("aaa=1.0001g;1.02;1.03;1.04;1.05"));
#else
  EXPECT_EQ("'aaa'=1.00009g;1.02;1.03;1.04;1.05", do_parse("aaa=1.0001g;1.02;1.03;1.04;1.05"));
#endif
}

TEST(PerfDataTest, problem_701_001) { EXPECT_EQ("'TotalGetRequests__Total'=0requests/s;;;0", do_parse("'TotalGetRequests__Total'=0.00requests/s;;;0;")); }
// 'TotalGetRequests__Total'=0.00requests/s;;;0;
TEST(PerfDataTest, value_various_reparse) {
  std::vector<std::string> strings;
  strings.emplace_back("'aaa'=1g;0;4;2;5");
  strings.emplace_back("'aaa'=6g;1;2;3;4");
  strings.emplace_back("'aaa'=6g;;2;3;4");
  strings.emplace_back("'aaa'=6g;1;;3;4");
  strings.emplace_back("'aaa'=6g;1;2;;4");
  strings.emplace_back("'aaa'=6g;1;2;3");
  strings.emplace_back("'aaa'=6g;;;3;4");
  strings.emplace_back("'aaa'=6g;1;;;4");
  strings.emplace_back("'aaa'=6g;1;2");
  strings.emplace_back("'aaa'=6g");
  for (const std::string& s : strings) {
    EXPECT_EQ(s.c_str(), do_parse(s));
  }
}

TEST(PerfDataTest, truncation) {
  const std::string s = "'abcdefghijklmnopqrstuv'=1g;0;4;2;5 'abcdefghijklmnopqrstuv'=1g;0;4;2;5";
  EXPECT_EQ(s.c_str(), do_parse(s, -1));
  EXPECT_EQ(s.c_str(), do_parse(s, 71));
  EXPECT_EQ("'abcdefghijklmnopqrstuv'=1g;0;4;2;5", do_parse(s, 70));
  EXPECT_EQ("'abcdefghijklmnopqrstuv'=1g;0;4;2;5", do_parse(s, 35));
  EXPECT_EQ("", do_parse(s, 34));
}

TEST(PerfDataTest, unit_conversion_b) {
  const auto d = str::format::convert_to_byte_units(1234567890, "B");
  ASSERT_DOUBLE_EQ(1234567890, d);
}
TEST(PerfDataTest, unit_conversion_k) {
  const auto d = str::format::convert_to_byte_units(1234567890, "K");
  ASSERT_DOUBLE_EQ(1205632.705078125, d);
}
TEST(PerfDataTest, unit_conversion_m) {
  const auto d = str::format::convert_to_byte_units(1234567890, "M");
  ASSERT_DOUBLE_EQ(1177.3756885528564, d);
}
TEST(PerfDataTest, unit_conversion_g) {
  const auto d = str::format::convert_to_byte_units(1234567890, "G");
  ASSERT_DOUBLE_EQ(1.1497809458523989, d);
}

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

// Status conversion tests
TEST(StatusConversionTest, gbp_to_nagios_status) {
  EXPECT_EQ(NSCAPI::query_return_codes::returnOK, nscapi::protobuf::functions::gbp_to_nagios_status(PB::Common::ResultCode::OK));
  EXPECT_EQ(NSCAPI::query_return_codes::returnWARN, nscapi::protobuf::functions::gbp_to_nagios_status(PB::Common::ResultCode::WARNING));
  EXPECT_EQ(NSCAPI::query_return_codes::returnCRIT, nscapi::protobuf::functions::gbp_to_nagios_status(PB::Common::ResultCode::CRITICAL));
  EXPECT_EQ(NSCAPI::query_return_codes::returnUNKNOWN, nscapi::protobuf::functions::gbp_to_nagios_status(PB::Common::ResultCode::UNKNOWN));
}

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

// Performance data extraction tests
TEST(PerfDataExtractionTest, extract_perf_value_as_string_float) {
  PB::Common::PerformanceData perf;
  perf.mutable_float_value()->set_value(42.5);
  perf.mutable_float_value()->set_unit("ms");

  const auto result = nscapi::protobuf::functions::extract_perf_value_as_string(perf);
  EXPECT_EQ("42.5", result);
}

TEST(PerfDataExtractionTest, extract_perf_value_as_string_with_unit) {
  PB::Common::PerformanceData perf;
  perf.mutable_float_value()->set_value(100);
  perf.mutable_float_value()->set_unit("K");

  const auto result = nscapi::protobuf::functions::extract_perf_value_as_string(perf);
  EXPECT_EQ("102400", result);
}

TEST(PerfDataExtractionTest, extract_perf_value_as_int) {
  PB::Common::PerformanceData perf;
  perf.mutable_float_value()->set_value(42.7);

  const auto result = nscapi::protobuf::functions::extract_perf_value_as_int(perf);
  EXPECT_EQ(42, result);
}

TEST(PerfDataExtractionTest, extract_perf_maximum_as_string) {
  PB::Common::PerformanceData perf;
  perf.mutable_float_value()->mutable_maximum()->set_value(100);

  const auto result = nscapi::protobuf::functions::extract_perf_maximum_as_string(perf);
  EXPECT_EQ("100", result);
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

// Append payload tests
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

TEST(AppendPayloadTest, append_simple_exec_response_payload) {
  PB::Commands::ExecuteResponseMessage::Response payload;
  nscapi::protobuf::functions::append_simple_exec_response_payload(&payload, "cmd", 2, "Error");

  EXPECT_EQ("cmd", payload.command());
  EXPECT_EQ("Error", payload.message());
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, payload.result());
}

TEST(AppendPayloadTest, append_simple_submit_response_payload) {
  PB::Commands::SubmitResponseMessage::Response payload;
  nscapi::protobuf::functions::append_simple_submit_response_payload(&payload, "cmd", true, "OK");

  EXPECT_EQ("cmd", payload.command());
  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_OK, payload.result().code());
  EXPECT_EQ("OK", payload.result().message());
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

TEST(AppendPayloadTest, append_simple_exec_request_payload) {
  PB::Commands::ExecuteRequestMessage::Request payload;
  const std::vector<std::string> args = {"arg1"};
  nscapi::protobuf::functions::append_simple_exec_request_payload(&payload, "cmd", args);

  EXPECT_EQ("cmd", payload.command());
  EXPECT_EQ(1, payload.arguments_size());
}

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

// Additional set_response_good_wdata tests
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

// parse_nagios tests
TEST(StatusConversionTest, parse_nagios_ok) {
  EXPECT_EQ(PB::Common::ResultCode::OK, nscapi::protobuf::functions::parse_nagios("ok"));
  EXPECT_EQ(PB::Common::ResultCode::OK, nscapi::protobuf::functions::parse_nagios("OK"));
  EXPECT_EQ(PB::Common::ResultCode::OK, nscapi::protobuf::functions::parse_nagios("o"));
  EXPECT_EQ(PB::Common::ResultCode::OK, nscapi::protobuf::functions::parse_nagios("0"));
}

TEST(StatusConversionTest, parse_nagios_warning) {
  EXPECT_EQ(PB::Common::ResultCode::WARNING, nscapi::protobuf::functions::parse_nagios("warn"));
  EXPECT_EQ(PB::Common::ResultCode::WARNING, nscapi::protobuf::functions::parse_nagios("WARNING"));
  EXPECT_EQ(PB::Common::ResultCode::WARNING, nscapi::protobuf::functions::parse_nagios("w"));
  EXPECT_EQ(PB::Common::ResultCode::WARNING, nscapi::protobuf::functions::parse_nagios("1"));
}

TEST(StatusConversionTest, parse_nagios_critical) {
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, nscapi::protobuf::functions::parse_nagios("crit"));
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, nscapi::protobuf::functions::parse_nagios("CRITICAL"));
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, nscapi::protobuf::functions::parse_nagios("c"));
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, nscapi::protobuf::functions::parse_nagios("2"));
}

TEST(StatusConversionTest, parse_nagios_unknown) {
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, nscapi::protobuf::functions::parse_nagios("unknown"));
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, nscapi::protobuf::functions::parse_nagios("invalid"));
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, nscapi::protobuf::functions::parse_nagios("3"));
}

// nagios_status_to_gpb tests
TEST(StatusConversionTest, nagios_status_to_gpb) {
  EXPECT_EQ(PB::Common::ResultCode::OK, nscapi::protobuf::functions::nagios_status_to_gpb(NSCAPI::query_return_codes::returnOK));
  EXPECT_EQ(PB::Common::ResultCode::WARNING, nscapi::protobuf::functions::nagios_status_to_gpb(NSCAPI::query_return_codes::returnWARN));
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, nscapi::protobuf::functions::nagios_status_to_gpb(NSCAPI::query_return_codes::returnCRIT));
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, nscapi::protobuf::functions::nagios_status_to_gpb(NSCAPI::query_return_codes::returnUNKNOWN));
}

// Additional copy_response tests
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

// Performance data string value extraction test
TEST(PerfDataExtractionTest, extract_perf_value_as_string_string_value) {
  PB::Common::PerformanceData perf;
  perf.mutable_string_value()->set_value("test_string");

  const auto result = nscapi::protobuf::functions::extract_perf_value_as_string(perf);
  EXPECT_EQ("test_string", result);
}

TEST(PerfDataExtractionTest, extract_perf_value_as_string_no_value) {
  PB::Common::PerformanceData perf;

  const auto result = nscapi::protobuf::functions::extract_perf_value_as_string(perf);
  EXPECT_EQ("unknown", result);
}

TEST(PerfDataExtractionTest, extract_perf_value_as_int_string_value) {
  PB::Common::PerformanceData perf;
  perf.mutable_string_value()->set_value("test");

  const auto result = nscapi::protobuf::functions::extract_perf_value_as_int(perf);
  EXPECT_EQ(0, result);
}

TEST(PerfDataExtractionTest, extract_perf_value_as_int_with_unit) {
  PB::Common::PerformanceData perf;
  perf.mutable_float_value()->set_value(1);
  perf.mutable_float_value()->set_unit("K");

  const auto result = nscapi::protobuf::functions::extract_perf_value_as_int(perf);
  EXPECT_EQ(1024, result);
}

TEST(PerfDataExtractionTest, extract_perf_maximum_as_string_with_unit) {
  PB::Common::PerformanceData perf;
  perf.mutable_float_value()->mutable_maximum()->set_value(1);
  perf.mutable_float_value()->set_unit("M");

  const auto result = nscapi::protobuf::functions::extract_perf_maximum_as_string(perf);
  EXPECT_EQ("1048576", result);
}

TEST(PerfDataExtractionTest, extract_perf_maximum_as_string_no_float) {
  PB::Common::PerformanceData perf;

  const auto result = nscapi::protobuf::functions::extract_perf_maximum_as_string(perf);
  EXPECT_EQ("unknown", result);
}

// Query response edge cases
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

// Unit conversion tests for T (Terabytes)
TEST(PerfDataTest, unit_conversion_t) {
  const auto d = str::format::convert_to_byte_units(1099511627776, "T");
  ASSERT_DOUBLE_EQ(1.0, d);
}

// Tests for malformed input handling
TEST(ParseFunctionsTest, parse_simple_submit_response_malformed_input) {
  std::string response;
  const bool result = nscapi::protobuf::functions::parse_simple_submit_response("not valid protobuf data", response);

  EXPECT_FALSE(result);
  EXPECT_EQ("Failed to parse submit response message", response);
}

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

TEST(ParseFunctionsTest, parse_simple_exec_response_malformed_input) {
  std::list<std::string> result;
  const auto ret = nscapi::protobuf::functions::parse_simple_exec_response("not valid protobuf data", result);

  EXPECT_EQ(NSCAPI::query_return_codes::returnUNKNOWN, ret);
  EXPECT_TRUE(result.empty());
}

TEST(ParseFunctionsTest, parse_simple_query_response_empty_string) {
  std::string msg, perf;
  const auto ret = nscapi::protobuf::functions::parse_simple_query_response("", msg, perf, 0);

  EXPECT_EQ(NSCAPI::query_return_codes::returnUNKNOWN, ret);
}

TEST(ParseFunctionsTest, parse_simple_exec_response_empty_string) {
  std::list<std::string> result;
  const auto ret = nscapi::protobuf::functions::parse_simple_exec_response("", result);

  // Empty string may parse as empty message
  EXPECT_TRUE(result.empty());
}

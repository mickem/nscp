// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

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

// Additional submit request tests
TEST(SubmitRequestTest, create_simple_submit_request_ok) {
  std::string buffer;
  nscapi::protobuf::functions::create_simple_submit_request("channel", "cmd", NSCAPI::query_return_codes::returnOK, "OK message", "", buffer);

  PB::Commands::SubmitRequestMessage message;
  ASSERT_TRUE(message.ParseFromString(buffer));
  EXPECT_EQ(PB::Common::ResultCode::OK, message.payload(0).result());
}

TEST(SubmitRequestTest, create_simple_submit_request_critical) {
  std::string buffer;
  nscapi::protobuf::functions::create_simple_submit_request("channel", "cmd", NSCAPI::query_return_codes::returnCRIT, "Critical", "metric=100", buffer);

  PB::Commands::SubmitRequestMessage message;
  ASSERT_TRUE(message.ParseFromString(buffer));
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, message.payload(0).result());
}

TEST(SubmitRequestTest, create_simple_submit_request_unknown) {
  std::string buffer;
  nscapi::protobuf::functions::create_simple_submit_request("channel", "cmd", NSCAPI::query_return_codes::returnUNKNOWN, "Unknown", "", buffer);

  PB::Commands::SubmitRequestMessage message;
  ASSERT_TRUE(message.ParseFromString(buffer));
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, message.payload(0).result());
}

TEST(SubmitRequestTest, create_simple_submit_request_empty_perf) {
  std::string buffer;
  nscapi::protobuf::functions::create_simple_submit_request("channel", "cmd", NSCAPI::query_return_codes::returnOK, "Message", "", buffer);

  PB::Commands::SubmitRequestMessage message;
  ASSERT_TRUE(message.ParseFromString(buffer));
  EXPECT_EQ(0, message.payload(0).lines(0).perf_size());
}

TEST(SubmitRequestTest, create_simple_submit_request_with_perf_data) {
  std::string buffer;
  nscapi::protobuf::functions::create_simple_submit_request("channel", "cmd", NSCAPI::query_return_codes::returnOK, "Message", "cpu=50%", buffer);

  PB::Commands::SubmitRequestMessage message;
  ASSERT_TRUE(message.ParseFromString(buffer));
  EXPECT_GT(message.payload(0).lines(0).perf_size(), 0);
  EXPECT_EQ("cpu", message.payload(0).lines(0).perf(0).alias());
}

// Additional submit response payload tests
TEST(AppendPayloadTest, append_simple_submit_response_payload_error) {
  PB::Commands::SubmitResponseMessage::Response payload;
  nscapi::protobuf::functions::append_simple_submit_response_payload(&payload, "cmd", false, "Error occurred");

  EXPECT_EQ("cmd", payload.command());
  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_ERROR, payload.result().code());
  EXPECT_EQ("Error occurred", payload.result().message());
}

TEST(AppendPayloadTest, append_simple_submit_response_payload_empty_message) {
  PB::Commands::SubmitResponseMessage::Response payload;
  nscapi::protobuf::functions::append_simple_submit_response_payload(&payload, "cmd", true, "");

  EXPECT_EQ("cmd", payload.command());
  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_OK, payload.result().code());
  EXPECT_EQ("", payload.result().message());
}

// Parse submit response edge cases
TEST(SubmitResponseTest, parse_simple_submit_response_empty_string) {
  std::string response_msg;
  // Empty string parses as empty protobuf message with 0 payloads, which throws THROW_INVALID_SIZE
  EXPECT_THROW(nscapi::protobuf::functions::parse_simple_submit_response("", response_msg), std::exception);
}

namespace {
void add_submit_payload(PB::Commands::SubmitResponseMessage &message, bool ok, const std::string &msg) {
  auto *payload = message.add_payload();
  payload->set_command("cmd");
  payload->mutable_result()->set_code(ok ? PB::Common::Result_StatusCodeType_STATUS_OK : PB::Common::Result_StatusCodeType_STATUS_ERROR);
  payload->mutable_result()->set_message(msg);
}
}  // namespace

TEST(SubmitResponseTest, parse_multi_submit_response_all_ok) {
  PB::Commands::SubmitResponseMessage message;
  add_submit_payload(message, true, "first ok");
  add_submit_payload(message, true, "second ok");

  std::string response_msg;
  EXPECT_TRUE(nscapi::protobuf::functions::parse_multi_submit_response(message.SerializeAsString(), response_msg));
  EXPECT_EQ("first ok", response_msg);
}

TEST(SubmitResponseTest, parse_multi_submit_response_partial_failure) {
  // The order matters: the failure comes FIRST, mirroring channel=NSCA,GRAPHITE
  // where the NSCA failure used to be masked by GRAPHITE's later OK.
  PB::Commands::SubmitResponseMessage message;
  add_submit_payload(message, false, "connection refused");
  add_submit_payload(message, true, "submitted ok");

  std::string response_msg;
  EXPECT_FALSE(nscapi::protobuf::functions::parse_multi_submit_response(message.SerializeAsString(), response_msg));
  EXPECT_EQ("connection refused", response_msg);
}

TEST(SubmitResponseTest, parse_multi_submit_response_collects_all_failures) {
  PB::Commands::SubmitResponseMessage message;
  add_submit_payload(message, false, "first error");
  add_submit_payload(message, true, "ok");
  add_submit_payload(message, false, "second error");

  std::string response_msg;
  EXPECT_FALSE(nscapi::protobuf::functions::parse_multi_submit_response(message.SerializeAsString(), response_msg));
  EXPECT_EQ("first error, second error", response_msg);
}

TEST(SubmitResponseTest, parse_multi_submit_response_single_payload_matches_simple) {
  std::string buffer;
  nscapi::protobuf::functions::create_simple_submit_response_ok("channel", "command", "Success", buffer);

  std::string response_msg;
  EXPECT_TRUE(nscapi::protobuf::functions::parse_multi_submit_response(buffer, response_msg));
  EXPECT_EQ("Success", response_msg);
}

TEST(SubmitResponseTest, parse_multi_submit_response_no_payloads_is_an_error) {
  // Unlike parse_simple_submit_response this must not throw: an empty reply
  // (every handler failed to answer) is an ordinary failure to report.
  std::string response_msg;
  EXPECT_FALSE(nscapi::protobuf::functions::parse_multi_submit_response("", response_msg));
  EXPECT_EQ("Submit response contained no status", response_msg);
}

TEST(SubmitResponseTest, parse_simple_submit_response_error_status) {
  PB::Commands::SubmitResponseMessage message;
  auto* payload = message.add_payload();
  payload->set_command("cmd");
  payload->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_ERROR);
  payload->mutable_result()->set_message("Error message");

  const std::string buffer = message.SerializeAsString();
  std::string response_msg;
  const auto result = nscapi::protobuf::functions::parse_simple_submit_response(buffer, response_msg);

  EXPECT_FALSE(result);
  EXPECT_EQ("Error message", response_msg);
}

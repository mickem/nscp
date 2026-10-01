// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <nscapi/protobuf/functions_response.hpp>
#include <str/utf8.hpp>

namespace nscapi {
namespace protobuf {

void functions::set_response_good(::PB::Commands::QueryResponseMessage_Response &response, std::string message) {
  response.set_result(PB::Common::ResultCode::OK);
  response.add_lines()->set_message(message);
}

void functions::set_response_good_wdata(::PB::Commands::QueryResponseMessage_Response &response, std::string message) {
  response.set_result(PB::Common::ResultCode::OK);
  response.set_data(message);
  response.add_lines()->set_message("see data segment");
}

void functions::set_response_good_wdata(::PB::Commands::SubmitResponseMessage_Response &response, std::string message) {
  response.mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
  response.mutable_result()->set_data(message);
  response.mutable_result()->set_message("see data segment");
}

void functions::set_response_good_wdata(::PB::Commands::ExecuteResponseMessage_Response &response, std::string message) {
  response.set_result(PB::Common::ResultCode::OK);
  response.set_data(message);
  response.set_message("see data segment");
  if (response.command().empty()) response.set_command("unknown");
}

void functions::set_response_good(::PB::Commands::SubmitResponseMessage_Response &response, std::string message) {
  response.mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
  response.mutable_result()->set_message(message);
  if (response.command().empty()) response.set_command("unknown");
}

void functions::set_response_good(::PB::Commands::ExecuteResponseMessage_Response &response, std::string message) {
  response.set_result(PB::Common::ResultCode::OK);
  response.set_message(message);
  if (response.command().empty()) response.set_command("unknown");
}

void functions::set_response_bad(::PB::Commands::QueryResponseMessage_Response &response, std::string message) {
  response.set_result(PB::Common::ResultCode::UNKNOWN);
  response.add_lines()->set_message(message);
  if (response.command().empty()) response.set_command("unknown");
}

void functions::set_response_bad(::PB::Commands::SubmitResponseMessage::Response &response, std::string message) {
  response.mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_ERROR);
  response.mutable_result()->set_message(message);
  if (response.command().empty()) response.set_command("unknown");
}

void functions::set_response_bad(::PB::Commands::ExecuteResponseMessage::Response &response, std::string message) {
  response.set_result(PB::Common::ResultCode::UNKNOWN);
  response.set_message(message);
  if (response.command().empty()) response.set_command("unknown");
}

namespace {
void make_valid(std::string *text) {
  if (!utf8::is_valid(*text)) *text = utf8::make_valid(*text);
}
}  // namespace

void functions::make_valid_utf8(::PB::Commands::QueryResponseMessage &message) {
  for (auto &payload : *message.mutable_payload()) {
    for (auto &line : *payload.mutable_lines()) {
      make_valid(line.mutable_message());
      for (auto &perf : *line.mutable_perf()) make_valid(perf.mutable_alias());
    }
  }
}

void functions::make_valid_utf8(::PB::Commands::ExecuteResponseMessage &message) {
  for (auto &payload : *message.mutable_payload()) make_valid(payload.mutable_message());
}

void functions::make_valid_utf8(::PB::Commands::SubmitResponseMessage &message) {
  for (auto &payload : *message.mutable_payload()) {
    if (payload.has_result()) make_valid(payload.mutable_result()->mutable_message());
  }
}

}  // namespace protobuf
}  // namespace nscapi

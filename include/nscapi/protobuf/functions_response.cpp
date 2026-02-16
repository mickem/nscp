/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <nscapi/protobuf/functions_response.hpp>

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

}  // namespace protobuf
}  // namespace nscapi

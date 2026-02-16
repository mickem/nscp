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

#include <NSCAPI.h>

#include <nscapi/protobuf/functions_perfdata.hpp>
#include <nscapi/protobuf/functions_status.hpp>
#include <nscapi/protobuf/functions_submit.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <str/xtos.hpp>

#define THROW_INVALID_SIZE(size) \
  throw nsclient::nsclient_exception(std::string("Whoops, invalid payload size: ") + str::xtos(size) + " != 1 at line " + str::xtos(__LINE__));

namespace nscapi {
namespace protobuf {

void functions::create_simple_submit_request(std::string channel, std::string command, const NSCAPI::nagiosReturn ret, std::string msg, const std::string &perf,
                                             std::string &buffer) {
  PB::Commands::SubmitRequestMessage message;
  message.set_channel(channel);

  auto *payload = message.add_payload();
  payload->set_command(command);
  payload->set_result(nagios_status_to_gpb(ret));
  auto *l = payload->add_lines();
  l->set_message(msg);
  if (!perf.empty()) parse_performance_data(l, perf);

  message.SerializeToString(&buffer);
}

void functions::create_simple_submit_response_ok(const std::string &channel, const std::string &command, const std::string &msg, std::string &buffer) {
  PB::Commands::SubmitResponseMessage message;

  auto *payload = message.add_payload();
  payload->set_command(command);
  payload->mutable_result()->set_message(msg);
  payload->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
  message.SerializeToString(&buffer);
}

bool functions::parse_simple_submit_response(const std::string &request, std::string &response) {
  PB::Commands::SubmitResponseMessage message;
  if (!message.ParseFromString(request)) {
    response = "Failed to parse submit response message";
    return false;
  }

  if (message.payload_size() != 1) {
    THROW_INVALID_SIZE(message.payload_size());
  }
  auto payload = message.payload().Get(0);
  response = payload.mutable_result()->message();
  return payload.mutable_result()->code() == PB::Common::Result_StatusCodeType_STATUS_OK;
}

void functions::append_simple_submit_response_payload(PB::Commands::SubmitResponseMessage::Response *payload, std::string command, bool result,
                                                      std::string msg) {
  payload->set_command(command);
  payload->mutable_result()->set_code(result ? PB::Common::Result_StatusCodeType_STATUS_OK : PB::Common::Result_StatusCodeType_STATUS_ERROR);
  payload->mutable_result()->set_message(msg);
}

}  // namespace protobuf
}  // namespace nscapi

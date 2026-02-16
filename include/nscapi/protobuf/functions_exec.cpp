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

#include <nscapi/protobuf/functions_exec.hpp>
#include <nscapi/protobuf/functions_query.hpp>
#include <nscapi/protobuf/functions_status.hpp>

namespace nscapi {
namespace protobuf {

void functions::create_simple_exec_request(const std::string &module, const std::string &command, const std::list<std::string> &args, std::string &request) {
  PB::Commands::ExecuteRequestMessage message;
  if (!module.empty()) {
    auto *kvp = message.mutable_header()->add_metadata();
    kvp->set_key("target");
    kvp->set_value(module);
  }

  auto *payload = message.add_payload();
  payload->set_command(command);

  for (const std::string &s : args) payload->add_arguments(s);

  message.SerializeToString(&request);
}

void functions::create_simple_exec_request(const std::string &module, const std::string &command, const std::vector<std::string> &args, std::string &request) {
  PB::Commands::ExecuteRequestMessage message;
  if (!module.empty()) {
    auto *kvp = message.mutable_header()->add_metadata();
    kvp->set_key("target");
    kvp->set_value(module);
  }

  auto *payload = message.add_payload();
  payload->set_command(command);

  for (const std::string &s : args) payload->add_arguments(s);

  message.SerializeToString(&request);
}

int functions::parse_simple_exec_response(const std::string &response, std::list<std::string> &result) {
  int ret = 0;
  PB::Commands::ExecuteResponseMessage message;
  if (!message.ParseFromString(response)) {
    return NSCAPI::query_return_codes::returnUNKNOWN;
  }

  for (int i = 0; i < message.payload_size(); i++) {
    result.push_back(message.payload(i).message());
    const int r = gbp_to_nagios_status(message.payload(i).result());
    if (r > ret) ret = r;
  }
  return ret;
}

int functions::create_simple_exec_response(const std::string &command, NSCAPI::nagiosReturn ret, const std::string &result, std::string &response) {
  PB::Commands::ExecuteResponseMessage message;

  auto *payload = message.add_payload();
  payload->set_command(command);
  payload->set_message(result);

  payload->set_result(nagios_status_to_gpb(ret));
  message.SerializeToString(&response);
  return NSCAPI::cmd_return_codes::isSuccess;
}

int functions::create_simple_exec_response_unknown(std::string command, std::string result, std::string &response) {
  PB::Commands::ExecuteResponseMessage message;

  auto *payload = message.add_payload();
  payload->set_command(command);
  payload->set_message(result);

  payload->set_result(nagios_status_to_gpb(NSCAPI::exec_return_codes::returnERROR));
  message.SerializeToString(&response);
  return NSCAPI::cmd_return_codes::isSuccess;
}

void functions::append_simple_exec_response_payload(PB::Commands::ExecuteResponseMessage::Response *payload, std::string command, int ret, std::string msg) {
  payload->set_command(command);
  payload->set_message(msg);
  payload->set_result(nagios_status_to_gpb(ret));
}

void functions::append_simple_exec_request_payload(PB::Commands::ExecuteRequestMessage::Request *payload, std::string command,
                                                   const std::vector<std::string> &arguments) {
  payload->set_command(command);
  for (const std::string &s : arguments) {
    payload->add_arguments(s);
  }
}

}  // namespace protobuf
}  // namespace nscapi

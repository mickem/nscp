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
#include <nscapi/protobuf/functions_query.hpp>
#include <nscapi/protobuf/functions_status.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <str/xtos.hpp>

#define THROW_INVALID_SIZE(size) \
  throw nsclient::nsclient_exception(std::string("Whoops, invalid payload size: ") + str::xtos(size) + " != 1 at line " + str::xtos(__LINE__));

namespace nscapi {
namespace protobuf {

void functions::create_simple_query_request(std::string command, const std::list<std::string> &arguments, std::string &buffer) {
  PB::Commands::QueryRequestMessage message;

  auto *payload = message.add_payload();
  payload->set_command(command);

  for (const std::string &s : arguments) {
    payload->add_arguments(s);
  }

  message.SerializeToString(&buffer);
}

void functions::create_simple_query_request(std::string command, const std::vector<std::string> &arguments, std::string &buffer) {
  PB::Commands::QueryRequestMessage message;

  auto *payload = message.add_payload();
  payload->set_command(command);

  for (const std::string &s : arguments) {
    payload->add_arguments(s);
  }

  message.SerializeToString(&buffer);
}

void functions::append_simple_query_response_payload(PB::Commands::QueryResponseMessage::Response *payload, std::string command, NSCAPI::nagiosReturn ret,
                                                     std::string msg, const std::string &perf) {
  payload->set_command(command);
  payload->set_result(nagios_status_to_gpb(ret));
  auto *l = payload->add_lines();
  l->set_message(msg);
  if (!perf.empty()) parse_performance_data(l, perf);
}

void functions::append_simple_query_request_payload(PB::Commands::QueryRequestMessage::Request *payload, std::string command,
                                                    const std::vector<std::string> &arguments) {
  payload->set_command(command);
  for (const std::string &s : arguments) {
    payload->add_arguments(s);
  }
}

void functions::parse_simple_query_request(std::list<std::string> &args, const std::string &request) {
  PB::Commands::QueryRequestMessage message;
  if (!message.ParseFromString(request)) {
    return;
  }

  if (message.payload_size() != 1) {
    THROW_INVALID_SIZE(message.payload_size());
  }
  const auto payload = message.payload().Get(0);
  for (int i = 0; i < payload.arguments_size(); i++) {
    args.push_back(payload.arguments(i));
  }
}

int functions::parse_simple_query_response(const std::string &response, std::string &msg, std::string &perf, std::size_t max_length) {
  PB::Commands::QueryResponseMessage message;
  if (!message.ParseFromString(response)) {
    return NSCAPI::query_return_codes::returnUNKNOWN;
  }

  if (message.payload_size() == 0 || message.payload(0).lines_size() == 0) {
    return NSCAPI::query_return_codes::returnUNKNOWN;
  }
  if (message.payload_size() > 1 && message.payload(0).lines_size() > 1) {
    THROW_INVALID_SIZE(message.payload_size());
  }

  const auto payload = message.payload().Get(0);
  for (const auto &l : payload.lines()) {
    msg += l.message();
    std::string tmpPerf = build_performance_data(l, max_length);
    if (!tmpPerf.empty()) {
      if (perf.empty()) {
        perf = tmpPerf;
      } else {
        perf += " " + tmpPerf;
      }
    }
  }
  return gbp_to_nagios_status(payload.result());
}

std::string functions::query_data_to_nagios_string(const PB::Commands::QueryResponseMessage &message, std::size_t max_length) {
  std::stringstream ss;
  for (int i = 0; i < message.payload_size(); ++i) {
    const auto &p = message.payload(i);
    for (int j = 0; j < p.lines_size(); ++j) {
      const auto &l = p.lines(j);
      if (l.perf_size() > 0)
        ss << l.message() << "|" << build_performance_data(l, max_length);
      else
        ss << l.message();
    }
  }
  return ss.str();
}

std::string functions::query_data_to_nagios_string(const PB::Commands::QueryResponseMessage::Response &p, std::size_t max_length) {
  std::stringstream ss;
  for (int j = 0; j < p.lines_size(); ++j) {
    const auto &l = p.lines(j);
    if (l.perf_size() > 0)
      ss << l.message() << "|" << build_performance_data(l, max_length);
    else
      ss << l.message();
  }
  return ss.str();
}

}  // namespace protobuf
}  // namespace nscapi

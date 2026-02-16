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

#include <nscapi/protobuf/functions_copy.hpp>
#include <nscapi/protobuf/functions_perfdata.hpp>
#include <nscapi/protobuf/functions_query.hpp>
#include <nscapi/protobuf/functions_status.hpp>

namespace nscapi {
namespace protobuf {

void functions::copy_response(const std::string &command, ::PB::Commands::QueryResponseMessage::Response *target,
                              const ::PB::Commands::ExecuteResponseMessage::Response &source) {
  auto *line = target->add_lines();
  line->set_message(source.message());
  target->set_command(command);
}

void functions::copy_response(const std::string &command, ::PB::Commands::QueryResponseMessage::Response *target,
                              const ::PB::Commands::SubmitResponseMessage::Response &source) {
  auto *line = target->add_lines();
  line->set_message(source.result().message());
  target->set_command(command);
  target->set_result(gbp_status_to_gbp_nagios(source.result().code()));
}

void functions::copy_response(const std::string &command, ::PB::Commands::QueryResponseMessage::Response *target,
                              const ::PB::Commands::QueryResponseMessage::Response &source) {
  target->CopyFrom(source);
}

void functions::copy_response(const std::string &command, ::PB::Commands::ExecuteResponseMessage::Response *target,
                              const ::PB::Commands::ExecuteResponseMessage::Response &source) {
  target->CopyFrom(source);
}

void functions::copy_response(const std::string &command, ::PB::Commands::ExecuteResponseMessage::Response *target,
                              const ::PB::Commands::SubmitResponseMessage::Response &source) {
  target->set_message(source.result().message());
  target->set_command(source.command());
  target->set_result(gbp_status_to_gbp_nagios(source.result().code()));
}

void functions::copy_response(const std::string &command, ::PB::Commands::ExecuteResponseMessage::Response *target,
                              const ::PB::Commands::QueryResponseMessage::Response &source) {
  target->set_message(query_data_to_nagios_string(source, no_truncation));
  target->set_command(source.command());
  target->set_result(source.result());
}

void functions::copy_response(const std::string &command, ::PB::Commands::SubmitResponseMessage::Response *target,
                              const ::PB::Commands::ExecuteResponseMessage::Response &source) {
  target->mutable_result()->set_message(source.message());
}

void functions::copy_response(const std::string &command, ::PB::Commands::SubmitResponseMessage::Response *target,
                              const ::PB::Commands::SubmitResponseMessage::Response &source) {
  target->CopyFrom(source);
}

void functions::copy_response(const std::string &command, ::PB::Commands::SubmitResponseMessage::Response *target,
                              const ::PB::Commands::QueryResponseMessage::Response &source) {
  target->mutable_result()->set_message(query_data_to_nagios_string(source, no_truncation));
  target->mutable_result()->set_code(gbp_to_nagios_gbp_status(source.result()));
}

}  // namespace protobuf
}  // namespace nscapi

// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <nscapi/protobuf/functions_convert.hpp>
#include <nscapi/protobuf/functions_exec.hpp>
#include <nscapi/protobuf/functions_query.hpp>
#include <nscapi/protobuf/functions_status.hpp>

namespace nscapi {
namespace protobuf {

void functions::make_submit_from_query(std::string &message, const std::string &channel, const std::string &alias, const std::string &target,
                                       const std::string &source) {
  PB::Commands::QueryResponseMessage response;
  response.ParseFromString(message);
  PB::Commands::SubmitRequestMessage request;
  request.mutable_header()->CopyFrom(response.header());
  request.mutable_header()->set_source_id(request.mutable_header()->recipient_id());
  for (int i = 0; i < request.mutable_header()->hosts_size(); i++) {
    auto *host = request.mutable_header()->mutable_hosts(i);
    if (host->id() == request.mutable_header()->recipient_id()) {
      host->clear_address();
      host->clear_metadata();
    }
  }
  request.set_channel(channel);
  if (!target.empty()) request.mutable_header()->set_recipient_id(target);
  if (!source.empty()) {
    request.mutable_header()->set_sender_id(source);
    bool found = false;
    for (int i = 0; i < request.mutable_header()->hosts_size(); i++) {
      auto *host = request.mutable_header()->mutable_hosts(i);
      if (host->id() == source) {
        host->set_address(source);
        found = true;
      }
    }
    if (!found) {
      auto *host = request.mutable_header()->add_hosts();
      host->set_id(source);
      host->set_address(source);
    }
  }
  for (int i = 0; i < response.payload_size(); ++i) {
    request.add_payload()->CopyFrom(response.payload(i));
    if (!alias.empty()) request.mutable_payload(i)->set_alias(alias);
  }
  message = request.SerializeAsString();
}

void functions::make_query_from_exec(std::string &data) {
  PB::Commands::ExecuteResponseMessage exec_response_message;
  exec_response_message.ParseFromString(data);
  PB::Commands::QueryResponseMessage query_response_message;
  query_response_message.mutable_header()->CopyFrom(exec_response_message.header());
  for (int i = 0; i < exec_response_message.payload_size(); ++i) {
    const auto &p = exec_response_message.payload(i);
    append_simple_query_response_payload(query_response_message.add_payload(), p.command(), p.result(), p.message());
  }
  data = query_response_message.SerializeAsString();
}

void functions::make_query_from_submit(std::string &data) {
  PB::Commands::SubmitResponseMessage submit_response_message;
  submit_response_message.ParseFromString(data);
  PB::Commands::QueryResponseMessage query_response_message;
  query_response_message.mutable_header()->CopyFrom(submit_response_message.header());
  for (int i = 0; i < submit_response_message.payload_size(); ++i) {
    const auto &p = submit_response_message.payload(i);
    append_simple_query_response_payload(query_response_message.add_payload(), p.command(), gbp_status_to_gbp_nagios(p.result().code()), p.result().message(),
                                         "");
  }
  data = query_response_message.SerializeAsString();
}

void functions::make_exec_from_submit(std::string &data) {
  PB::Commands::SubmitResponseMessage submit_response_message;
  submit_response_message.ParseFromString(data);
  PB::Commands::ExecuteResponseMessage exec_response_message;
  exec_response_message.mutable_header()->CopyFrom(submit_response_message.header());
  for (int i = 0; i < submit_response_message.payload_size(); ++i) {
    const auto &p = submit_response_message.payload(i);
    append_simple_exec_response_payload(exec_response_message.add_payload(), p.command(), gbp_status_to_gbp_nagios(p.result().code()), p.result().message());
  }
  data = exec_response_message.SerializeAsString();
}

void functions::make_return_header(PB::Common::Header *target, const PB::Common::Header &source) {
  target->CopyFrom(source);
  target->set_source_id(target->recipient_id());
}

}  // namespace protobuf
}  // namespace nscapi

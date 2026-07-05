// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <client/command_line_parser.hpp>
#include <memory>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/nscapi_targets.hpp>
#include <nscapi/protobuf/command.hpp>

#include "op5_client.hpp"

class Op5Client : public nscapi::impl::simple_plugin {
 private:
  std::string channel_;

  std::shared_ptr<op5_client> client;

 public:
  Op5Client();
  virtual ~Op5Client();
  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();

  bool commandLineExec(const int target_mode, const PB::Commands::ExecuteRequestMessage::Request &request,
                       PB::Commands::ExecuteResponseMessage::Response *response, const PB::Commands::ExecuteRequestMessage &request_message);
  void handleNotification(const std::string &channel, const PB::Commands::SubmitRequestMessage &request_message,
                          PB::Commands::SubmitResponseMessage *response_message);

 private:
  bool cli_add(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response);
  bool cli_install(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response);
};

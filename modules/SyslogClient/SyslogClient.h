// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <client/command_line_parser.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/nscapi_targets.hpp>
#include <nscapi/protobuf/command.hpp>

namespace po = boost::program_options;
namespace sh = nscapi::settings_helper;

class SyslogClient : public nscapi::impl::simple_plugin {
 private:
  std::string channel_;
  std::string hostname_;

  client::configuration client_;

 public:
  SyslogClient();
  virtual ~SyslogClient();
  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();

  void query_fallback(const PB::Commands::QueryRequestMessage &request_message, PB::Commands::QueryResponseMessage &response_message);
  bool commandLineExec(const int target_mode, const PB::Commands::ExecuteRequestMessage &request, PB::Commands::ExecuteResponseMessage &response);
  void handleNotification(const std::string &channel, const PB::Commands::SubmitRequestMessage &request_message,
                          PB::Commands::SubmitResponseMessage *response_message);

 private:
  void add_command(std::string key, std::string args);
  void add_target(std::string key, std::string args);
  std::string parse_priority(std::string severity_arg, std::string facility_arg);
};

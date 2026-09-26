// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <client/command_line_parser.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/nscapi_targets.hpp>
#include <nscapi/protobuf/command.hpp>

namespace po = boost::program_options;
namespace sh = nscapi::settings_helper;

class NSCAClient : public nscapi::impl::simple_plugin {
  std::string channel_;
  std::string hostname_;
  std::string encoding_;

  client::configuration client_;

 public:
  NSCAClient();
  virtual ~NSCAClient();
  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();

  void query_fallback(const PB::Commands::QueryRequestMessage &request_message, PB::Commands::QueryResponseMessage &response_message);
  bool commandLineExec(int target_mode, const PB::Commands::ExecuteRequestMessage &request, PB::Commands::ExecuteResponseMessage &response);
  void handleNotification(const std::string &channel, const PB::Commands::SubmitRequestMessage &request_message,
                          PB::Commands::SubmitResponseMessage *response_message);

 private:
  void add_command(const std::string &key, const std::string &args);
  void add_target(const std::string &key, const std::string &args);
  // `nscp nsca install`: point this agent's NSCA submissions at a server, in
  // one command instead of three `nscp settings --set` calls. Writes the
  // default target (address, port, encryption and the shared key) and enables
  // the module. An unspecified option keeps whatever is on disk, so a re-run to
  // change one thing does not reset the rest.
  bool cli_install(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response) const;
};

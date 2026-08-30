// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <client/command_line_parser.hpp>
#include <memory>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/nscapi_targets.hpp>
#include <nscapi/protobuf/command.hpp>

namespace po = boost::program_options;
namespace sh = nscapi::settings_helper;

// Defined in syslog_client.hpp, which needs the client machinery included
// first; a forward declaration keeps that ordering out of this header. The
// destructor that destroys the shared_ptr lives in the .cpp, where the type
// is complete.
namespace syslog_client {
struct syslog_client_handler;
}

class SyslogClient : public nscapi::impl::simple_plugin {
 private:
  std::string channel_;
  std::string hostname_;

  // Declared before client_: client_ is constructed with this handler.
  std::shared_ptr<syslog_client::syslog_client_handler> handler_;
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

// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <client/command_line_parser.hpp>
#include <net/check_mk/lua/lua_check_mk.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <scripts/script_nscp.hpp>

#include "check_mk_client.hpp"
#include "check_mk_handler.hpp"

namespace po = boost::program_options;
namespace sh = nscapi::settings_helper;

class CheckMKClient : public nscapi::impl::simple_plugin {
 private:
  std::shared_ptr<lua::lua_runtime> lua_runtime_;
  std::shared_ptr<scripts::nscp::nscp_runtime_impl> nscp_runtime_;
  boost::filesystem::path root_;
  std::string channel_;
  std::string hostname_;
  std::string encoding_;

  std::shared_ptr<check_mk_client::check_mk_client_handler> handler_;
  client::configuration client_;

 public:
  CheckMKClient();
  virtual ~CheckMKClient();
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
  bool add_script(std::string alias, std::string file);
};

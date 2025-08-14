/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <check_mk/lua/lua_check_mk.hpp>
#include <client/command_line_parser.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <scripts/script_nscp.hpp>

#include "check_mk_client.hpp"
#include "check_mk_handler.hpp"

namespace po = boost::program_options;
namespace sh = nscapi::settings_helper;

class CheckMKClient : public nscapi::impl::simple_plugin {
 private:
  boost::shared_ptr<lua::lua_runtime> lua_runtime_;
  boost::shared_ptr<scripts::nscp::nscp_runtime_impl> nscp_runtime_;
  boost::filesystem::path root_;
  std::string channel_;
  std::string hostname_;
  std::string encoding_;

  boost::shared_ptr<check_mk_client::check_mk_client_handler> handler_;
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

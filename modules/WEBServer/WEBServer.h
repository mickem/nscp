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

#include <Server.h>

#include <boost/shared_ptr.hpp>
#include <client/simple_client.hpp>
#include <nscapi/nscapi_protobuf_command.hpp>
#include <nscapi/nscapi_protobuf_log.hpp>
#include <nscapi/nscapi_protobuf_metrics.hpp>
#include <nscapi/plugin.hpp>

#include "error_handler_interface.hpp"
#include "session_manager_interface.hpp"
#include "user_config.hpp"

class WEBServer : public nscapi::impl::simple_plugin {
  typedef std::map<std::string, std::string> role_map;

 public:
  WEBServer();
  virtual ~WEBServer();
  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);

  void ensure_role(role_map &roles, const nscapi::settings_helper::settings_registry &settings, const std::string &role_path, const std::string &role,
                   const std::string &value, const std::string &reason);
  void ensure_user(const nscapi::settings_helper::settings_registry &settings, const std::string &path, const std::string &user, const std::string &role,
                   const std::string &value, const std::string &reason);

  bool unloadModule();
  void handleLogMessage(const PB::Log::LogEntry::Entry &message);
  bool commandLineExec(const int target_mode, const PB::Commands::ExecuteRequestMessage::Request &request,
                       PB::Commands::ExecuteResponseMessage::Response *response, const PB::Commands::ExecuteRequestMessage &request_message);
  void submitMetrics(const PB::Metrics::MetricsMessage &response) const;
  bool install_server(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response);
  bool cli_add_user(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response);
  bool cli_add_role(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response);
  bool password(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response);

 private:
  void add_user(const std::string &key, const std::string &arg);

  boost::shared_ptr<error_handler_interface> log_handler;
  boost::shared_ptr<client::cli_client> client;
  boost::shared_ptr<session_manager_interface> session;
  boost::shared_ptr<Mongoose::Server> server;

  web_server::user_config users_;
};

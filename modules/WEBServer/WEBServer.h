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


#include "user_config.hpp"

#include "session_manager_interface.hpp"
#include "error_handler_interface.hpp"

#include <Server.h>

#include <client/simple_client.hpp>

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/plugin.hpp>

#include <boost/shared_ptr.hpp>

class WEBServer : public nscapi::impl::simple_plugin {
	typedef std::map<std::string, std::string> role_map;

public:
	WEBServer();
	virtual ~WEBServer();
	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);

	void ensure_role(role_map &roles, nscapi::settings_helper::settings_registry &settings, std::string role_path, std::string role, std::string value, std::string reason);
	void ensure_user(nscapi::settings_helper::settings_registry &settings, std::string path, std::string user, std::string role, std::string value, std::string reason);

	bool unloadModule();
	void handleLogMessage(const Plugin::LogEntry::Entry &message);
	bool commandLineExec(const int target_mode, const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message);
	void submitMetrics(const Plugin::MetricsMessage &response);
	bool install_server(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response);
	bool cli_add_user(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response);
	bool cli_add_role(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response);
	bool password(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response);
private:

	void add_user(std::string key, std::string arg);

	boost::shared_ptr<error_handler_interface> log_handler;
	boost::shared_ptr<client::cli_client> client;
	boost::shared_ptr<session_manager_interface> session;
	boost::shared_ptr<Mongoose::Server> server;

	web_server::user_config users_;

};

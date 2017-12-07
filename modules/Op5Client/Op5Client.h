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

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/nscapi_targets.hpp>

#include <client/command_line_parser.hpp>

namespace po = boost::program_options;
namespace sh = nscapi::settings_helper;

class Op5Client : public nscapi::impl::simple_plugin {
private:

	std::string channel_;
	std::string hostname_;


public:
	Op5Client();
	virtual ~Op5Client();
	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	bool commandLineExec(const int target_mode, const Plugin::ExecuteRequestMessage &request, Plugin::ExecuteResponseMessage &response);
	void handleNotification(const std::string &channel, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage *response_message);

	void onEvent(const Plugin::EventMessage &request, const std::string &buffer);

private:
	void add_command(std::string key, std::string args);

	bool has_host(std::string host);
	bool add_host(std::string host);
	bool remove_host(std::string host);
	bool send_host_check(std::string host, int status_code, std::string msg, std::string &status, bool create_if_missing = true);
	bool send_service_check(std::string host, std::string service, int status_code, std::string msg, std::string &status, bool create_if_missing = true);
	std::pair<bool, bool> has_service(std::string service, std::string host, std::string &hosts_string);
	bool add_host_to_service(std::string service, std::string host, std::string &hosts_string);
	bool add_service(std::string host, std::string service);
	bool save_config();

	void register_host(std::string host);
	void deregister_host(std::string host);



	std::string op5_url;
	std::string op5_username;
	std::string op5_password;
	bool deregister;

	std::string hostgroups_;
	std::string contactgroups_;
};

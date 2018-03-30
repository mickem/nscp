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

#include "script_interface.hpp"

#include "commands.hpp"
#include "alias.hpp"

#include <nscapi/nscapi_plugin_impl.hpp>

#include <string>

class CheckExternalScripts : public nscapi::impl::simple_plugin {
private:
	boost::shared_ptr<script_provider_interface> provider_;
	alias::command_handler aliases_;
	unsigned int timeout;
	std::string root_;
	bool allowArgs_;
	bool allowNasty_;

public:
	CheckExternalScripts();
	virtual ~CheckExternalScripts();
	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();
	void query_fallback(const Plugin::QueryRequestMessage_Request &request, Plugin::QueryResponseMessage_Response *response, const Plugin::QueryRequestMessage &request_message);
	bool commandLineExec(const int target_mode, const Plugin::ExecuteRequestMessage_Request &request, Plugin::ExecuteResponseMessage_Response *response, const Plugin::ExecuteRequestMessage &request_message);

private:

	void handle_command(const commands::command_object &cd, const std::list<std::string> &args, Plugin::QueryResponseMessage_Response *response);
	void handle_alias(const alias::command_object &cd, const std::list<std::string> &args, Plugin::QueryResponseMessage_Response *response);
	void addAllScriptsFrom(std::string path);
	void add_command(std::string key, std::string arg);
	void add_alias(std::string key, std::string command);
	void add_wrapping(std::string key, std::string command);
};

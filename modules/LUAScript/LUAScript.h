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

#include <scripts/functions.hpp>
#include <scripts/script_interface.hpp>
#include <scripts/script_nscp.hpp>

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>

#include <str/xtos.hpp>

#include <lua/lua_script.hpp>
#include <lua/lua_core.hpp>

#include <boost/optional.hpp>
#include <boost/scoped_ptr.hpp>


class LUAScript : public nscapi::impl::simple_plugin {
private:
	boost::scoped_ptr<scripts::script_manager<lua::lua_traits> > scripts_;
	boost::shared_ptr<lua::lua_runtime> lua_runtime_;
	boost::shared_ptr<scripts::nscp::nscp_runtime_impl> nscp_runtime_;
	boost::filesystem::path root_;

public:
	LUAScript() {}
	virtual ~LUAScript() {}
	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);

	bool unloadModule();
	void query_fallback(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &request_message);
	bool commandLineExec(const int target_mode, const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message);
	void handleNotification(const std::string &channel, const Plugin::QueryResponseMessage::Response &request, Plugin::SubmitResponseMessage::Response *response, const Plugin::SubmitRequestMessage &request_message);

	bool reload(std::wstring &msg);

	bool loadScript(std::string alias, std::string file);
	//	NSCAPI::nagiosReturn execute_and_load(std::list<std::wstring> args, std::wstring &message);
	//	NSCAPI::nagiosReturn handleSimpleNotification(const std::wstring channel, const std::wstring source, const std::wstring command, NSCAPI::nagiosReturn code, std::wstring msg, std::wstring perf);
	//	NSCAPI::nagiosReturn handleRAWCommand(const wchar_t* char_command, const std::string &request, std::string &response);
	//	NSCAPI::nagiosReturn commandLineExec(const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &result);
};

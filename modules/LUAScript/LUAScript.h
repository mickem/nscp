/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <boost/optional.hpp>
#include <boost/scoped_ptr.hpp>

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <strEx.h>
#include <utils.h>
#include <scripts/functions.hpp>
#include <scripts/script_interface.hpp>
#include <scripts/script_nscp.hpp>

#include <lua/lua_script.hpp>
#include <lua/lua_core.hpp>

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

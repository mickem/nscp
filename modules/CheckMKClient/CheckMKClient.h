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

#include <boost/tuple/tuple.hpp>

#include <nscapi/nscapi_protobuf.hpp>
#include <client/command_line_parser.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/nscapi_targets.hpp>
#include <nscapi/nscapi_protobuf_types.hpp>
#include <socket/client.hpp>

#include <check_mk/lua/lua_check_mk.hpp>

namespace po = boost::program_options;
namespace sh = nscapi::settings_helper;

class CheckMKClient : public nscapi::impl::simple_plugin {
private:
	boost::scoped_ptr<scripts::script_manager<lua::lua_traits> > scripts_;
	boost::shared_ptr<lua::lua_runtime> lua_runtime_;
	boost::shared_ptr<scripts::nscp::nscp_runtime_impl> nscp_runtime_;
	boost::filesystem::path root_;
	std::string channel_;
	std::string hostname_;
	std::string encoding_;

	client::configuration client_;

public:
	CheckMKClient();
	virtual ~CheckMKClient();
	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	void query_fallback(const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message);
	bool commandLineExec(const int target_mode, const Plugin::ExecuteRequestMessage &request, Plugin::ExecuteResponseMessage &response);
	void handleNotification(const std::string &channel, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage *response_message);

private:

	void add_command(std::string key, std::string args);
	void add_target(std::string key, std::string args);
	bool add_script(std::string alias, std::string file);
};

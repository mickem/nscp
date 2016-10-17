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

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/nscapi_protobuf_types.hpp>
#include <nscapi/nscapi_targets.hpp>

#include <client/command_line_parser.hpp>

namespace po = boost::program_options;
namespace sh = nscapi::settings_helper;

class SMTPClient : public nscapi::impl::simple_plugin {
private:

	std::string channel_;
	client::configuration client_;

public:
	SMTPClient();
	virtual ~SMTPClient();
	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	void query_fallback(const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message);
	bool commandLineExec(const int target_mode, const Plugin::ExecuteRequestMessage &request, Plugin::ExecuteResponseMessage &response);
	void handleNotification(const std::string &channel, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage *response_message);

private:
	void add_command(std::string key, std::string args);
	void add_target(std::string key, std::string args);
};

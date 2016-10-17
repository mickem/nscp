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

class CheckHelpers : public nscapi::impl::simple_plugin {
public:
	CheckHelpers() {}
	virtual ~CheckHelpers() {}

	// Check commands
	void check_critical(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void check_warning(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void check_multi(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void check_version(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void check_always_warning(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void check_always_critical(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void check_ok(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void check_always_ok(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void check_negate(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void check_timeout(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void check_and_forward(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);

	void filter_perf(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void render_perf(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void xform_perf(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);

	// Helpers
	void check_change_status(::Plugin::Common_ResultCode status, const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	bool simple_query(const std::string &command, const std::vector<std::string> &arguments, Plugin::QueryResponseMessage::Response *response);
	bool simple_query(const std::string &command, const std::list<std::string> &arguments, Plugin::QueryResponseMessage::Response *response);
};
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

class ElasticClient : public nscapi::impl::simple_plugin {
private:

	bool started;

	std::string channel_;
	std::string hostname_;

	std::string address;

	std::string event_index;
	std::string event_type;

	std::string metrics_index;
	std::string metrics_type;

	std::string nsclient_index;
	std::string nsclient_type;


public:
	ElasticClient();
	virtual ~ElasticClient();
	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	void query_fallback(const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message);
	bool commandLineExec(const int target_mode, const Plugin::ExecuteRequestMessage &request, Plugin::ExecuteResponseMessage &response);
	void handleNotification(const std::string &channel, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage *response_message);

	void submitMetrics(const Plugin::MetricsMessage &response);
	void onEvent(const Plugin::EventMessage &request, const std::string &buffer);

	void handleLogMessage(const Plugin::LogEntry::Entry &message);

private:
	void add_command(std::string key, std::string args);
	void add_target(std::string key, std::string args);
};

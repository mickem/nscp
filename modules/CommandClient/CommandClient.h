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

#include <boost/scoped_ptr.hpp>

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/plugin.hpp>
#include <client/simple_client.hpp>

struct client_handler : public client::cli_handler {
private:
	nscapi::core_wrapper* core;
	int plugin_id;

public:
	client_handler(nscapi::core_wrapper* core, int plugin_id) : core(core), plugin_id(plugin_id) {}
	int get_plugin_id() const {
		return plugin_id;
	}
	nscapi::core_wrapper* get_core() const {
		return core;
	}
	virtual void output_message(const std::string &msg);
	virtual void log_debug(std::string module, std::string file, int line, std::string msg) const;
	virtual void log_error(std::string module, std::string file, int line, std::string msg) const;
};

class CommandClient : public nscapi::impl::simple_plugin {
	boost::scoped_ptr<client::cli_client> client;
public:
	CommandClient() {}
	virtual ~CommandClient() {}
	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();
	void handleLogMessage(const Plugin::LogEntry::Entry &message);
	bool commandLineExec(const int target_mode, const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message);
	void submitMetrics(const Plugin::MetricsMessage &response);

};

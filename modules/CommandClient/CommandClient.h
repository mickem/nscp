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

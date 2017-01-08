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

#include <map>
#include <error/error.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <process/execute_process.hpp>
#include "commands.hpp"
#include "alias.hpp"

class CheckExternalScripts : public nscapi::impl::simple_plugin {
private:
	commands::command_handler commands_;
	alias::command_handler aliases_;
	unsigned int timeout;
	//std::string commands_path;
	//std::string aliases_path;
	std::string scriptDirectory_;
	std::string root_;
	bool allowArgs_;
	bool allowNasty_;
	std::map<std::string, std::string> wrappings_;

public:
	CheckExternalScripts();
	virtual ~CheckExternalScripts();
	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();
	void query_fallback(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &request_message);
	bool commandLineExec(const int target_mode, const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message);

private:

	void add_script(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response);

	void handle_command(const commands::command_object &cd, const std::list<std::string> &args, Plugin::QueryResponseMessage::Response *response);
	void handle_alias(const alias::command_object &cd, const std::list<std::string> &args, Plugin::QueryResponseMessage::Response *response);
	void addAllScriptsFrom(std::string path);
	void add_command(std::string key, std::string arg);
	void add_alias(std::string key, std::string command);
	void add_wrapping(std::string key, std::string command);
	std::string generate_wrapped_command(std::string command);
	void configure(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response);
	void list(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response);
};

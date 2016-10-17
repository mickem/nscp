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

#include <string>
#include <list>
#include <vector>

#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/dll_defines.hpp>

namespace nscapi {
	class NSCAPI_EXPORT core_helper {
		const nscapi::core_wrapper *core_;
		int plugin_id_;
	public:
		core_helper(const nscapi::core_wrapper *core, int plugin_id) : core_(core), plugin_id_(plugin_id) {}
		void register_command(std::string command, std::string description, std::list<std::string> aliases = std::list<std::string>());
		void unregister_command(std::string command);
		void register_alias(std::string command, std::string description, std::list<std::string> aliases = std::list<std::string>());
		void register_channel(const std::string channel);

		NSCAPI::nagiosReturn simple_query(const std::string command, const std::list<std::string> & argument, std::string & message, std::string & perf);
		bool simple_query(const std::string command, const std::list<std::string> & argument, std::string & result);
		bool simple_query(const std::string command, const std::vector<std::string> & argument, std::string & result);
		NSCAPI::nagiosReturn simple_query_from_nrpe(const std::string command, const std::string & buffer, std::string & message, std::string & perf);

		NSCAPI::nagiosReturn exec_simple_command(const std::string target, const std::string command, const std::list<std::string> &argument, std::list<std::string> & result);
		bool submit_simple_message(const std::string channel, const std::string source_id, const std::string target_id, const std::string command, const NSCAPI::nagiosReturn code, const std::string & message, const std::string & perf, std::string & response);

	private:
		const nscapi::core_wrapper* get_core();
	};
}
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

#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/dll_defines.hpp>

#include <string>
#include <list>
#include <vector>
#include <map>

namespace nscapi {
	class NSCAPI_EXPORT core_helper {
		const nscapi::core_wrapper *core_;
		int plugin_id_;
	public:
		core_helper(const nscapi::core_wrapper *core, int plugin_id) : core_(core), plugin_id_(plugin_id) {}
		void register_command(std::string command, std::string description, std::list<std::string> aliases = std::list<std::string>());
		void unregister_command(std::string command);
		void register_alias(std::string command, std::string description, std::list<std::string> aliases = std::list<std::string>());
		void register_event(const std::string event);
		void register_channel(const std::string channel);

		NSCAPI::nagiosReturn simple_query(const std::string command, const std::list<std::string> & argument, std::string & message, std::string & perf, std::size_t max_length);
		bool simple_query(const std::string command, const std::list<std::string> & argument, std::string & result);
		bool simple_query(const std::string command, const std::vector<std::string> & argument, std::string & result);
		NSCAPI::nagiosReturn simple_query_from_nrpe(const std::string command, const std::string & buffer, std::string & message, std::string & perf, std::size_t max_length);

		NSCAPI::nagiosReturn exec_simple_command(const std::string target, const std::string command, const std::list<std::string> &argument, std::list<std::string> & result);
		bool submit_simple_message(const std::string channel, const std::string source_id, const std::string target_id, const std::string command, const NSCAPI::nagiosReturn code, const std::string & message, const std::string & perf, std::string & response);
		bool emit_event(const std::string module, const std::string event, std::list<std::map<std::string, std::string> > data, std::string &error);
		bool emit_event(const std::string module, const std::string event, std::map<std::string, std::string> data, std::string &error);

		typedef std::map<std::string, std::string> storage_map;
		bool put_storage(std::string context, std::string key, std::string value, bool private_data, bool binary_data);
		storage_map get_storage_strings(std::string context);

		bool load_module(std::string name, std::string alias = "");
		bool unload_module(std::string name);

	private:
		const nscapi::core_wrapper* get_core();
	};
}
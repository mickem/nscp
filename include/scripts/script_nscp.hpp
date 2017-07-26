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

#include <scripts/script_interface.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>

namespace scripts {
	namespace nscp {
		struct tags {
			const static std::string simple_query_tag;
			const static std::string simple_exec_tag;
			const static std::string simple_submit_tag;
			const static std::string query_tag;
			const static std::string exec_tag;
			const static std::string submit_tag;
		};

		struct settings_provider_impl : public settings_provider {
			int plugin_id;
			nscapi::core_wrapper* core_;
			nscapi::settings_proxy settings_;
			settings_provider_impl(int plugin_id, nscapi::core_wrapper* core)
				: plugin_id(plugin_id)
				, core_(core)
				, settings_(plugin_id, core) {}

			virtual std::list<std::string> get_section(std::string section);
			virtual std::string get_string(std::string path, std::string key, std::string value);
			virtual void set_string(std::string path, std::string key, std::string value);
			virtual bool get_bool(std::string path, std::string key, bool value);
			virtual void set_bool(std::string path, std::string key, bool value);
			virtual int get_int(std::string path, std::string key, int value);
			virtual void set_int(std::string path, std::string key, int value);
			virtual void register_path(std::string path, std::string title, std::string description, bool advanced);
			virtual void register_key(std::string path, std::string key, std::string type, std::string title, std::string description, std::string defaultValue);
			virtual void save();
		};

		struct core_provider_impl : public core_provider {
			nscapi::core_wrapper* core_;
			core_provider_impl(nscapi::core_wrapper* core) : core_(core) {}

			virtual bool submit_simple_message(const std::string channel, const std::string command, const NSCAPI::nagiosReturn code, const std::string & message, const std::string & perf, std::string & response);
			virtual NSCAPI::nagiosReturn simple_query(const std::string &command, const std::list<std::string> & argument, std::string & msg, std::string & perf);
			virtual bool exec_simple_command(const std::string target, const std::string command, const std::list<std::string> &argument, std::list<std::string> & result);
			virtual bool exec_command(const std::string target, const std::string &request, std::string &response);
			virtual bool query(const std::string &request, std::string &response);
			virtual bool submit(const std::string target, const std::string &request, std::string &response);
			virtual bool reload(const std::string module);
			virtual void log(NSCAPI::log_level::level, const std::string file, int line, const std::string message);
		};
		struct nscp_runtime_impl : public nscp_runtime_interface {
			int plugin_id;
			nscapi::core_wrapper* core_;
			boost::shared_ptr<settings_provider_impl> settings_;
			boost::shared_ptr<core_provider_impl> core_provider_;

			nscp_runtime_impl(int plugin_id, nscapi::core_wrapper* core)
				: plugin_id(plugin_id)
				, core_(core)
				, settings_(new settings_provider_impl(plugin_id, core))
				, core_provider_(new core_provider_impl(core)) {}

			virtual void register_command(const std::string type, const std::string &command, const std::string &description);

			virtual boost::shared_ptr<settings_provider> get_settings_provider() {
				return settings_;
			}
			virtual boost::shared_ptr<core_provider> get_core_provider() {
				return core_provider_;
			}
		};
	}
}
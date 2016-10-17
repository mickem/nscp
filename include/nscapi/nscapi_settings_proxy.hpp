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

#include <list>

#include <settings/settings_core.hpp>
#include <settings/client/settings_client_interface.hpp>

#include <nscapi/nscapi_core_wrapper.hpp>

#include <nscapi/dll_defines.hpp>

namespace nscapi {
	class NSCAPI_EXPORT settings_proxy : public nscapi::settings_helper::settings_impl_interface {
	private:
		unsigned int plugin_id_;
		nscapi::core_wrapper* core_;

	public:
		settings_proxy(unsigned int plugin_id, nscapi::core_wrapper* core) : plugin_id_(plugin_id), core_(core) {}

		typedef std::list<std::string> string_list;

		virtual void register_path(std::string path, std::string title, std::string description, bool advanced, bool sample);
		virtual void register_key(std::string path, std::string key, int type, std::string title, std::string description, std::string defValue, bool advanced, bool sample);
		virtual void register_tpl(std::string path, std::string title, std::string icon, std::string description, std::string fields);

		virtual std::string get_string(std::string path, std::string key, std::string def);
		virtual void set_string(std::string path, std::string key, std::string value);
		virtual int get_int(std::string path, std::string key, int def);
		virtual void set_int(std::string path, std::string key, int value);
		virtual bool get_bool(std::string path, std::string key, bool def);
		virtual void set_bool(std::string path, std::string key, bool value);
		virtual string_list get_sections(std::string path);
		virtual string_list get_keys(std::string path);
		virtual std::string expand_path(std::string key);

		virtual void err(const char* file, int line, std::string message);
		virtual void warn(const char* file, int line, std::string message);
		virtual void info(const char* file, int line, std::string message);
		virtual void debug(const char* file, int line, std::string message);
		void save(const std::string context = "");
	};
}
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

#include <settings/settings_core.hpp>
#include <settings/settings_interface_impl.hpp>
#include <settings/client/settings_client_interface.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>

namespace settings_client {
	class settings_proxy : public nscapi::settings_helper::settings_impl_interface {
	private:
		settings::settings_handler_impl* core_;

	public:
		settings_proxy(settings::settings_handler_impl* core) : core_(core) {}

		typedef std::list<std::string> string_list;


		inline settings::settings_core* get_core() {
			return core_;
		}
		inline settings::instance_ptr get_impl() {
			return core_->get();
		}
		inline settings::settings_handler_impl* get_handler() {
			return core_;
		}
		virtual void register_path(std::string path, std::string title, std::string description, bool advanced, bool is_sample) {
			get_core()->register_path(0xffff, path, title, description, advanced, is_sample);
		}

		virtual void register_subkey(std::string path, std::string title, std::string description, bool advanced, bool is_sample) {
			get_core()->register_subkey(0xffff, path, title, description, advanced, is_sample);
		}

		virtual void register_key(std::string path, std::string key, std::string title, std::string description, std::string defValue, bool advanced, bool is_sample) {
			get_core()->register_key(0xffff, path, key, title, description, defValue, advanced, is_sample);
		}
		virtual void register_tpl(std::string path, std::string title, std::string icon, std::string description, std::string fields) {}


		virtual std::string get_string(std::string path, std::string key, std::string def) {
			return get_impl()->get_string(path, key, def);
		}
		virtual void set_string(std::string path, std::string key, std::string value) {
			get_impl()->set_string(path, key, value);
		}

		virtual string_list get_sections(std::string path) {
			return get_impl()->get_sections(path);
		}
		virtual string_list get_keys(std::string path) {
			return get_impl()->get_keys(path);
		}
		virtual std::string expand_path(std::string key) {
			return get_handler()->expand_path(key);
		}

		virtual void remove_key(std::string path, std::string key) {
			return get_impl()->remove_key(path, key);
		}
		virtual void remove_path(std::string path) {
			return get_impl()->remove_path(path);
		}


		virtual void err(const char* file, int line, std::string message) {
			get_core()->get_logger()->error("settings",file, line, message);
		}
		virtual void warn(const char* file, int line, std::string message) {
			get_core()->get_logger()->warning("settings",file, line, message);
		}
		virtual void info(const char* file, int line, std::string message)  {
			get_core()->get_logger()->info("settings",file, line, message);
		}
		virtual void debug(const char* file, int line, std::string message)  {
			get_core()->get_logger()->debug("settings",file, line, message);
		}
	};
}
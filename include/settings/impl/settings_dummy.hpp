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

#include <string>
#include <map>

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

#include <settings/settings_core.hpp>
#include <settings/settings_interface_impl.hpp>
//#define SI_CONVERT_ICU
#include <simpleini/simpleini.h>
#include <error/error.hpp>

namespace settings {
	class settings_dummy : public settings::settings_interface_impl {
	private:

	public:
		settings_dummy(settings::settings_core *core, std::string alias, std::string context) : settings::settings_interface_impl(core, alias, context) {}
		//////////////////////////////////////////////////////////////////////////
		/// Get a string value if it does not exist exception will be thrown
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @return the string value
		///
		/// @author mickem
		virtual op_string get_real_string(settings_core::key_path_type key) {
			return op_string();
		}
		//////////////////////////////////////////////////////////////////////////
		/// Get an integer value if it does not exist exception will be thrown
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @return the int value
		///
		/// @author mickem
		virtual op_int get_real_int(settings_core::key_path_type key) {
			return op_int();
		}
		//////////////////////////////////////////////////////////////////////////
		/// Get a boolean value if it does not exist exception will be thrown
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @return the boolean value
		///
		/// @author mickem
		virtual op_bool get_real_bool(settings_core::key_path_type key) {
			return op_bool();
		}
		//////////////////////////////////////////////////////////////////////////
		/// Check if a key exists
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @return true/false if the key exists.
		///
		/// @author mickem
		virtual bool has_real_key(settings_core::key_path_type key) {
			return false;
		}
		virtual bool has_real_path(std::string path) {
			return false;
		}
		//////////////////////////////////////////////////////////////////////////
		/// Write a value to the resulting context.
		///
		/// @param key The key to write to
		/// @param value The value to write
		///
		/// @author mickem
		virtual void set_real_value(settings_core::key_path_type key, conainer value) {}

		virtual void set_real_path(std::string path) {}
		virtual void remove_real_value(settings_core::key_path_type key) {}
		virtual void remove_real_path(std::string path) {}

		//////////////////////////////////////////////////////////////////////////
		/// Get all (sub) sections (given a path).
		/// If the path is empty all root sections will be returned
		///
		/// @param path The path to get sections from (if empty root sections will be returned)
		/// @param list The list to append nodes to
		/// @return a list of sections
		///
		/// @author mickem
		virtual void get_real_sections(std::string, string_list &) {}
		//////////////////////////////////////////////////////////////////////////
		/// Get all keys given a path/section.
		/// If the path is empty all root sections will be returned
		///
		/// @param path The path to get sections from (if empty root sections will be returned)
		/// @param list The list to append nodes to
		/// @return a list of sections
		///
		/// @author mickem
		virtual void get_real_keys(std::string, string_list &) {}
		//////////////////////////////////////////////////////////////////////////
		/// Save the settings store
		///
		/// @author mickem
		virtual void save() {
			settings_interface_impl::save();
		}
		virtual std::string get_info() {
			return "dummy settings";
		}
		settings::error_list validate() {
			settings::error_list ret;
			return ret;
		}
		virtual std::string get_type() { return "dummy"; }

	public:
		virtual void real_clear_cache() {}
		static bool context_exists(settings::settings_core*, std::string) {
			return true;
		}
		void ensure_exists() {}
	};
}
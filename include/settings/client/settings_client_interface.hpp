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

#include <map>
#include <list>

#include <boost/optional/optional.hpp>

#include <settings/settings_value.hpp>

namespace nscapi {
	namespace settings_helper {



		class settings_impl_interface {
		public:
			typedef std::list<std::string> string_list;

			//////////////////////////////////////////////////////////////////////////
			/// Register a path with the settings module.
			/// A registered key or path will be nicely documented in some of the settings files when converted.
			///
			/// @param path The path to register
			/// @param title The title to use
			/// @param description the description to use
			/// @param advanced advanced options will only be included if they are changed
			///
			/// @author mickem
			virtual void register_path(std::string path, std::string title, std::string description, bool advanced, bool sample) = 0;

			//////////////////////////////////////////////////////////////////////////
			/// Register a key with the settings module.
			/// A registered key or path will be nicely documented in some of the settings files when converted.
			///
			/// @param path The path to register
			/// @param key The key to register
			/// @param title The title to use
			/// @param description the description to use
			/// @param defValue the default value
			/// @param advanced advanced options will only be included if they are changed
			///
			/// @author mickem
			virtual void register_key(std::string path, std::string key, std::string title, std::string description, std::string defValue, bool advanced, bool sample) = 0;

			virtual void register_subkey(std::string path, std::string title, std::string description, bool advanced, bool sample) = 0;

			virtual void register_tpl(std::string path, std::string title, std::string icon, std::string description, std::string fields) = 0;

			//////////////////////////////////////////////////////////////////////////
			/// Get a string value if it does not exist the default value will be returned
			/// 
			/// @param path the path to look up
			/// @param key the key to lookup
			/// @param def the default value to use when no value is found
			/// @return the string value
			///
			/// @author mickem
			virtual std::string get_string(std::string path, std::string key, std::string def) = 0;
			//////////////////////////////////////////////////////////////////////////
			/// Set or update a string value
			///
			/// @param path the path to look up
			/// @param key the key to lookup
			/// @param value the value to set
			///
			/// @author mickem
			virtual void set_string(std::string path, std::string key, std::string value) = 0;

			// Meta Functions
			//////////////////////////////////////////////////////////////////////////
			/// Get all (sub) sections (given a path).
			/// If the path is empty all root sections will be returned
			///
			/// @param path The path to get sections from (if empty root sections will be returned)
			/// @return a list of sections
			///
			/// @author mickem
			virtual string_list get_sections(std::string path) = 0;
			//////////////////////////////////////////////////////////////////////////
			/// Get all keys for a path.
			///
			/// @param path The path to get keys under
			/// @return a list of keys
			///
			/// @author mickem
			virtual string_list get_keys(std::string path) = 0;


			virtual std::string expand_path(std::string key) = 0;


			virtual void remove_key(std::string path, std::string key) = 0;
			virtual void remove_path(std::string path) = 0;

			//////////////////////////////////////////////////////////////////////////
			/// Log an ERROR message.
			///
			/// @param file the file where the event happened
			/// @param line the line where the event happened
			/// @param message the message to log
			///
			/// @author mickem
			virtual void err(const char* file, int line, std::string message) = 0;
			//////////////////////////////////////////////////////////////////////////
			/// Log an WARNING message.
			///
			/// @param file the file where the event happened
			/// @param line the line where the event happened
			/// @param message the message to log
			///
			/// @author mickem
			virtual void warn(const char* file, int line, std::string message) = 0;
			//////////////////////////////////////////////////////////////////////////
			/// Log an INFO message.
			///
			/// @param file the file where the event happened
			/// @param line the line where the event happened
			/// @param message the message to log
			///
			/// @author mickem
			virtual void info(const char* file, int line, std::string message) = 0;
			//////////////////////////////////////////////////////////////////////////
			/// Log an DEBUG message.
			///
			/// @param file the file where the event happened
			/// @param line the line where the event happened
			/// @param message the message to log
			///
			/// @author mickem
			virtual void debug(const char* file, int line, std::string message) = 0;
		};
	}
}

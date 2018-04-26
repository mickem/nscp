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

#include <nsclient/logger/logger.hpp>

#include <str/xtos.hpp>
#include <str/wstring.hpp>

#include <simpleini/SimpleIni.h>

#include <iostream>
#include <fstream>
#include <string>
#include <map>

namespace settings {
	class OLDSettings : public settings::settings_interface_impl {
		std::string filename_;
		typedef std::pair<std::wstring, std::wstring> section_key_type;

		class settings_map : boost::noncopyable {
		public:

			typedef std::multimap<std::string, std::string> path_map;
			typedef std::multimap<settings_core::key_path_type, settings_core::key_path_type> key_map;
			typedef std::pair<std::string, std::string> section_key_type;
			typedef std::pair<settings_core::key_path_type, settings_core::key_path_type> keys_key_type;

			path_map sections_;
			key_map keys_;
			nsclient::logging::logger_instance logger_;

			settings_map(nsclient::logging::logger_instance logger) : logger_(logger) {}

			void read_map_file(std::string file) {
				std::ifstream in(file.c_str());
				if (!in) {
					logger_->error("settings", __FILE__, __LINE__, "Failed to read MAP file: " + utf8::cvt<std::string>(file));
					return;
				}
				in.exceptions(std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit);

				try {
					std::string tmp;
					while (true) {
						std::getline(in, tmp);
						parse_line(utf8::cvt<std::wstring>(tmp));
					}
				} catch (std::ifstream::failure e) {
					if (!in.eof())
						std::cerr << e.what() << '\n';
				}
			}
			void read_map_data(const std::string data) {
				BOOST_FOREACH(const std::string &l, str::utils::split_lst(data, std::string("\n"))) {
					parse_line(utf8::cvt<std::wstring>(l));
				}
			}
			void parse_line(std::wstring line) {
				strEx::replace(line, L"\n", L"");
				strEx::replace(line, L"\r", L"");
				std::wstring::size_type pos = line.find('#');
				if (pos != -1)
					line = line.substr(0, pos);
				pos = line.find_first_not_of(L" \t\n\r");
				if (pos == -1)
					return;
				line = line.substr(pos);
				pos = line.find('=');
				if (pos == -1) {
					logger_->error("settings", __FILE__, __LINE__, "Invalid syntax: " + utf8::cvt<std::string>(line));
					return;
				}
				std::pair<std::wstring, std::wstring> old_key = split_key(line.substr(0, pos));
				std::pair<std::wstring, std::wstring> new_key = split_key(line.substr(pos + 1));
				if (old_key.second == L"*" || old_key.second.empty()) {
					add(utf8::cvt<std::string>(line.substr(pos + 1)), utf8::cvt<std::string>(old_key.first));
				} else {
					add(utf8::cvt<std::string>(new_key.first), utf8::cvt<std::string>(new_key.second), utf8::cvt<std::string>(old_key.first), utf8::cvt<std::string>(old_key.second));
				}
			}
			std::pair<std::wstring, std::wstring> split_key(std::wstring key) {
				std::pair<std::wstring, std::wstring> ret;
				std::wstring::size_type pos = key.find_last_of('/');
				if (pos == -1)
					return std::pair<std::wstring, std::wstring>(key, L"");
				return std::pair<std::wstring, std::wstring>(key.substr(0, pos), key.substr(pos + 1));
			}

			void add(std::string path_new, std::string path_old) {
				sections_.insert(path_map::value_type(path_new, path_old));
			}
			void add(std::string path_new, std::string key_new, std::string path_old, std::string key_old) {
				settings_core::key_path_type new_key(path_new, key_new);
				settings_core::key_path_type old_key(path_old, key_old);
				keys_.insert(key_map::value_type(new_key, old_key));
			}
			std::string path(std::string path_new) {
				path_map::iterator it = sections_.find(path_new);
				if (it == sections_.end())
					return path_new;
				return (*it).second;
			}
			settings_core::key_path_type key(settings_core::key_path_type new_key) {
				key_map::iterator it1 = keys_.find(new_key);
				if (it1 != keys_.end()) {
					return (*it1).second;
				}
				path_map::iterator it2 = sections_.find(new_key.first);
				if (it2 != sections_.end()) {
					return settings_core::key_path_type((*it2).second, new_key.second);
				}
				return new_key;
			}
			std::string status() {
				return "Sections: " + str::xtos(sections_.size()) + ", "
					+ "Keys: " + str::xtos(keys_.size())
					;
			}

			void get_sections(std::string path, string_list &list) {
				std::wstring::size_type path_length = path.length();
				BOOST_FOREACH(section_key_type key, sections_) {
					if (path_length == 0 || path == "/") {
						std::string::size_type pos = key.first.find('/', 1);
						list.push_back(pos == std::string::npos ? key.first : key.first.substr(0, pos));
					} else if (key.first.length() > path_length && path == key.first.substr(0, path_length)) {
						std::string::size_type pos = key.first.find('/', path_length + 1);
						list.push_back(pos == std::string::npos ? key.first.substr(path_length + 1) : key.first.substr(path_length + 1, pos - path_length - 1));
					}
				}
				BOOST_FOREACH(keys_key_type key, keys_) {
					if (path.empty() || path == "/") {
						std::string::size_type pos = key.first.first.find('/', 1);
						if (pos != std::string::npos)
							key.first.first = key.first.first.substr(0, pos);
						list.push_back(key.first.first);
					} else if (key.first.first.length() > path_length && path == key.first.first.substr(0, path_length)) {
						std::string::size_type pos = key.first.first.find('/', path_length + 1);
						list.push_back(pos == std::wstring::npos ? key.first.first.substr(path_length + 1) : key.first.first.substr(path_length + 1, pos - path_length - 1));
					}
				}
				list.unique();
			}
		};

		settings_map map;
		typedef std::map<std::string, std::set<std::string> > section_cache_type;
		section_cache_type section_cache_;

	public:

		OLDSettings(settings::settings_core *core, std::string alias, std::string context) : settings::settings_interface_impl(core, alias, context), map(core->get_logger()) {
			get_logger()->debug("settings", __FILE__, __LINE__, "Loading OLD: " + context);
			std::string mapfile = "old-settings.map";
			std::string file = core->find_file("${exe-path}/" + mapfile, mapfile);
			bool readmap = false;
			if (boost::filesystem::exists(file)) {
				readmap = true;
				map.read_map_file(file);
			}
			if (!readmap) {
				get_logger()->error("settings", __FILE__, __LINE__, "Failed to read map file: " + mapfile);
			}

			string_list list = get_keys("/includes");
			BOOST_FOREACH(const std::string &key, list) {
				if (key.length() > 5 && key.substr(key.length() - 4, 4) == ".ini" && key.find_first_of(":/\\") == std::string::npos)
					add_child_unsafe(key, "old://${exe-path}/" + key);
				else
					add_child_unsafe(key, key);
			}
		}
		settings::error_list validate() {
			settings::error_list ret;
			return ret;
		}
		virtual void real_clear_cache() {}

		//////////////////////////////////////////////////////////////////////////
		/// Get a string value if it does not exist exception will be thrown
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @return the string value
		///
		/// @author mickem
		virtual op_string get_real_string(settings_core::key_path_type in_key) {
			settings_core::key_path_type key = map.key(in_key);
			if (has_key_int(key.first, key.second))
				return internal_get_value(key.first, key.second);
			if (has_key_int(in_key.first, in_key.second))
				return internal_get_value(in_key.first, in_key.second);
			return op_string();
		}
#define UNLIKELY_STRING L"$$$EMPTY_KEY$$$"

		std::string internal_get_value(std::string path, std::string key, int bufferSize = 1024) {
			TCHAR* buffer = new TCHAR[bufferSize + 2];
			if (buffer == NULL)
				throw settings_exception(__FILE__, __LINE__, "Out of memory error!");
			int retVal = GetPrivateProfileString(utf8::cvt<std::wstring>(path).c_str(), utf8::cvt<std::wstring>(key).c_str(), L"", buffer, bufferSize, utf8::cvt<std::wstring>(get_file_name()).c_str());
			if (retVal == bufferSize - 1) {
				delete[] buffer;
				return internal_get_value(path, key, bufferSize * 10);
			}
			std::string ret = utf8::cvt<std::string>(buffer);
			delete[] buffer;
			return ret;
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
			settings_core::key_path_type old = map.key(key);
			return has_key_int(old.first, old.second);
		}
		virtual bool has_real_path(std::string path) {
			return false;
		}

		std::set<std::string> internal_read_keys_from_section(std::string section, unsigned int bufferLength = 1024) {
			TCHAR* buffer = new TCHAR[bufferLength + 1];
			if (buffer == NULL)
				throw settings_exception(__FILE__, __LINE__, "internal_read_keys_from_section:: Failed to allocate memory for buffer!");
			unsigned int count = ::GetPrivateProfileSection(utf8::cvt<std::wstring>(section).c_str(), buffer, bufferLength, utf8::cvt<std::wstring>(get_file_name()).c_str());
			if (count == bufferLength - 2) {
				delete[] buffer;
				return internal_read_keys_from_section(section, bufferLength * 10);
			}
			std::set<std::string> ret;
			unsigned int last = 0;
			for (unsigned int i = 0; i < count; i++) {
				if (buffer[i] == '\0') {
					std::string s = utf8::cvt<std::string>(&buffer[last]);
					std::size_t p = s.find('=');
					ret.insert((p == std::string::npos) ? s : s.substr(0, p));
					last = i + 1;
				}
			}
			delete[] buffer;
			return ret;
		}

		bool has_key_int(std::string path, std::string key) {
			section_cache_type::const_iterator it = section_cache_.find(path);
			if (it == section_cache_.end()) {
				std::set<std::string> list = internal_read_keys_from_section(path);
				section_cache_[path] = list;
				it = section_cache_.find(path);
			}
			return (*it).second.find(key) != (*it).second.end();
		}

		//////////////////////////////////////////////////////////////////////////
		/// Write a value to the resulting context.
		///
		/// @param key The key to write to
		/// @param value The value to write
		///
		/// @author mickem
		virtual void set_real_value(settings_core::key_path_type key, conainer value) {
			try {
				key = map.key(key);
				WritePrivateProfileString(utf8::cvt<std::wstring>(key.first).c_str(), utf8::cvt<std::wstring>(key.second).c_str(), utf8::cvt<std::wstring>(value.get_string()).c_str(), utf8::cvt<std::wstring>(get_file_name()).c_str());
			} catch (settings_exception e) {
				get_logger()->error("settings", __FILE__, __LINE__, std::string("Failed to write key: " + e.reason()));
			} catch (...) {
				get_logger()->error("settings", __FILE__, __LINE__, "Unknown failure when writing key: " + make_skey(key.first, key.second));
			}
		}

		virtual void set_real_path(std::string path) {
			// NOT Supported (and not needed) so silently ignored!
		}
		virtual void remove_real_value(settings_core::key_path_type key) {
			// NOT Supported
		}
		virtual void remove_real_path(std::string path) {
			// NOT Supported
		}

		//////////////////////////////////////////////////////////////////////////
		/// Get all (sub) sections (given a path).
		/// If the path is empty all root sections will be returned
		///
		/// @param path The path to get sections from (if empty root sections will be returned)
		/// @param list The list to append nodes to
		/// @return a list of sections
		///
		/// @author mickem
		virtual void get_real_sections(std::string path, string_list &list) {
			map.get_sections(path, list);
			list.unique();
		}

		/**
		* Retrieves a list of section
		* @access public
		* @returns INIFile::sectionList
		* @qualifier
		* @param unsigned int bufferLength
		*/
		string_list int_read_sections(unsigned int bufferLength = BUFF_LEN) {
			string_list ret;
			TCHAR* buffer = new TCHAR[bufferLength + 1];
			if (buffer == NULL)
				throw settings_exception(__FILE__, __LINE__, "getSections:: Failed to allocate memory for buffer!");
			unsigned int count = ::GetPrivateProfileSectionNames(buffer, BUFF_LEN, utf8::cvt<std::wstring>(get_file_name()).c_str());
			if (count == bufferLength - 2) {
				delete[] buffer;
				return int_read_sections(bufferLength * 10);
			}
			unsigned int last = 0;
			for (unsigned int i = 0; i < count; i++) {
				if (buffer[i] == '\0') {
					std::string s = utf8::cvt<std::string>(&buffer[last]);
					ret.push_back(s);
					last = i + 1;
				}
			}
			delete[] buffer;
			return ret;
		}
		//////////////////////////////////////////////////////////////////////////
		/// Get all keys given a path/section.
		/// If the path is empty all root sections will be returned
		///
		/// @param path The path to get sections from (if empty root sections will be returned)
		/// @param list The list to append nodes to
		/// @return a list of sections
		///
		/// @author mickem
		virtual void get_real_keys(std::string path, string_list &list) {
			if (path.empty() || path == "/") {
				get_logger()->debug("settings", __FILE__, __LINE__, "Loose leaves not supported: TODO");
				return;
			}
			// @todo: this will NOT work for "nodes in paths"
			std::set<std::string> ignore_list;
			BOOST_FOREACH(settings_map::keys_key_type key, map.keys_) {
				if (key.first.first == path) {
					if (has_key_int(key.second.first, key.second.second)) {
						list.push_back(key.first.second);
						ignore_list.insert(key.second.first + "/" + key.second.second);
					}
				}
			}

			BOOST_FOREACH(settings_map::section_key_type key, map.sections_) {
				if (key.first == path) {
					section_cache_type::const_iterator it = section_cache_.find(key.second);
					if (it == section_cache_.end()) {
						std::set<std::string> list2 = internal_read_keys_from_section(key.second);
						section_cache_[path] = list2;
						it = section_cache_.find(path);
					}
					BOOST_FOREACH(const std::string &k, (*it).second) {
						std::set<std::string>::const_iterator cit = ignore_list.find(key.second + "/" + k);
						if (cit == ignore_list.end())
							list.push_back(k);
					}
				}
			}
		}
	private:

		void int_read_section(std::string section, string_list &list, unsigned int bufferLength = BUFF_LEN) {
			TCHAR* buffer = new TCHAR[bufferLength + 1];
			if (buffer == NULL)
				throw settings_exception(__FILE__, __LINE__, "getSections:: Failed to allocate memory for buffer!");
			unsigned int count = GetPrivateProfileSection(utf8::cvt<std::wstring>(section).c_str(), buffer, bufferLength, utf8::cvt<std::wstring>(get_file_name()).c_str());
			if (count == bufferLength - 2) {
				delete[] buffer;
				int_read_section(section, list, bufferLength * 10);
				return;
			}
			unsigned int last = 0;
			for (unsigned int i = 0; i < count; i++) {
				if (buffer[i] == '\0') {
					std::string s = utf8::cvt<std::string>(&buffer[last]);
					std::size_t p = s.find('=');
					if (p == std::wstring::npos)
						list.push_back(s);
					else
						list.push_back(s.substr(0, p));
					last = i + 1;
				}
			}
			delete[] buffer;
		}

		string_list int_read_section_from_inifile(std::string section, unsigned int bufferLength = BUFF_LEN) {
			TCHAR* buffer = new TCHAR[bufferLength + 1];
			if (buffer == NULL)
				throw settings_exception(__FILE__, __LINE__, "getSections:: Failed to allocate memory for buffer!");
			unsigned int count = GetPrivateProfileSection(utf8::cvt<std::wstring>(section).c_str(), buffer, bufferLength, utf8::cvt<std::wstring>(get_file_name()).c_str());
			if (count == bufferLength - 2) {
				delete[] buffer;
				return int_read_section_from_inifile(section, bufferLength * 10);
			}
			unsigned int last = 0;
			string_list list;
			for (unsigned int i = 0; i < count; i++) {
				if (buffer[i] == '\0') {
					std::string s = utf8::cvt<std::string>(&buffer[last]);
					std::size_t p = s.find('=');
					if (p == std::string::npos)
						list.push_back(s);
					else
						list.push_back(s.substr(0, p));
					last = i + 1;
				}
			}
			delete[] buffer;
			return list;
		}

		inline std::string get_file_name() {
			if (filename_.empty()) {
				filename_ = get_file_from_context();
			}
			return filename_;
		}
		bool file_exists() {
			return boost::filesystem::is_regular_file(get_file_name());
		}
		virtual std::string get_info() {
			return "INI settings: (" + context_ + ", " + get_file_name() + ")";
		}
		virtual std::string get_type() { return "old"; }
	public:
		static bool context_exists(settings::settings_core *core, std::string key) {
			net::url url = net::parse(key);
			std::string file = url.host + url.path;
			std::string tmp = core->expand_path(file);
			return boost::filesystem::exists(tmp);
		}
		void ensure_exists() {
			return;
		}
	};
}

#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <settings/settings_core.hpp>
#include <simpleini/SimpleIni.h>
//#include <settings/macros.h>

#include <strEx.h>


//#define MAIN_MODULES_SECTION_OLD _T("modules")
//#define MAIN_SECTION_TITLE _T("Settings")
//#define MAIN_STRING_LENGTH _T("string_length")

namespace settings {
	class OLDSettings : public settings::SettingsInterfaceImpl {
		std::wstring filename_;
		typedef std::pair<std::wstring,std::wstring> section_key_type;

		class settings_map : boost::noncopyable {
		public:

			typedef std::map<std::wstring,std::wstring> path_map;
			typedef std::map<settings_core::key_path_type,settings_core::key_path_type> key_map;
			typedef std::pair<std::wstring,std::wstring> section_key_type;
			typedef std::pair<settings_core::key_path_type,settings_core::key_path_type> keys_key_type;

			OLDSettings *parent;
			path_map sections_;
			key_map keys_;

			settings_map(OLDSettings *parent) : parent(parent) {}

			void read_map_file(std::wstring file) {
				parent->get_logger()->debug(__FILE__, __LINE__, _T("Reading MAP file: ") + file);

				std::ifstream in(strEx::wstring_to_string(file).c_str());
				if(!in) {
					parent->get_logger()->err(__FILE__, __LINE__, _T("Failed to read MAP file: ") + file);
					return;
				}
				in.exceptions(std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit);

				try{
					std::string tmp;
					while(true) {
						std::getline(in,tmp);
						parse_line(utf8::cvt<std::wstring>(tmp));
					}
				}
				catch(std::ifstream::failure e){
					if(!in.eof())
						std::cerr << e.what() <<'\n';
				}
			}
			void parse_line(std::wstring line) {
				int pos = line.find('#');
				if (pos != -1)
					line = line.substr(0, pos);
				pos = line.find_first_not_of(_T(" \t"));
				if (pos == -1)
					return;
				line = line.substr(pos);
				pos = line.find('=');
				if (pos == -1) {
					parent->get_logger()->err(__FILE__, __LINE__, _T("Invalid syntax: ") + line);
					return;
				}
				std::pair<std::wstring,std::wstring> old_key = split_key(line.substr(0, pos));
				std::pair<std::wstring,std::wstring> new_key = split_key(line.substr(pos+1));
				if (old_key.second == _T("*") || old_key.second.empty()) {
					add(line.substr(pos+1), old_key.first);
				} else {
					add(new_key.first, new_key.second, old_key.first, old_key.second);
				}

			}
			std::pair<std::wstring,std::wstring> split_key(std::wstring key) {
				std::pair<std::wstring,std::wstring> ret;
				int pos = key.find_last_of('/');
				if (pos == -1)
					return std::pair<std::wstring,std::wstring>(key, _T(""));
				return std::pair<std::wstring,std::wstring>(key.substr(0, pos), key.substr(pos+1));
			}

			void add(std::wstring path_new, std::wstring path_old) {
				sections_[path_new] = path_old;
			}
			void add(std::wstring path_new, std::wstring key_new, std::wstring path_old, std::wstring key_old) {
				settings_core::key_path_type new_key(path_new, key_new);
				settings_core::key_path_type old_key(path_old, key_old);
				keys_[new_key] = old_key;
			}
			std::wstring path(std::wstring path_new) {
				path_map::iterator it = sections_.find(path_new);
				if (it == sections_.end())
					return path_new;
				return (*it).second;
			}
			settings_core::key_path_type key(settings_core::key_path_type new_key) {
				key_map::iterator it1 = keys_.find(new_key);
				if (it1 != keys_.end()) {
					parent->get_logger()->quick_debug(new_key.first + _T(".") + new_key.second + _T(" not found in alias list"));
					return (*it1).second;
				}
				path_map::iterator it2 = sections_.find(new_key.first);
				if (it2 != sections_.end())
					return settings_core::key_path_type((*it2).second, new_key.second);
				return new_key;
			}

			void get_sections(std::wstring path, string_list &list) {
				unsigned int path_length = path.length();
				BOOST_FOREACH(section_key_type key, sections_) {
					if (path_length == 0 || path == _T("/")) {
						std::wstring::size_type pos = key.first.find(L'/', 1);
						list.push_back(pos == std::wstring::npos?key.first:key.first.substr(0,pos));
					} else if (key.first.length() > path_length && path == key.first.substr(0, path_length)) {
						std::wstring::size_type pos = key.first.find(L'/', path_length+1);
						list.push_back(pos == std::wstring::npos?key.first.substr(path_length+1):key.first.substr(path_length+1,pos-path_length-1));
					}
				}
				BOOST_FOREACH(keys_key_type key, keys_) {
					if (path.empty() || path == _T("/")) {
						std::wstring::size_type pos = key.first.first.find(L'/', 1);
						if (pos != std::wstring::npos)
							key.first.first = key.first.first.substr(0,pos);
						list.push_back(key.first.first);
					} else if (key.first.first.length() > path_length && path == key.first.first.substr(0, path_length)) {
						std::wstring::size_type pos = key.first.first.find(L'/', path_length+1);
						list.push_back(pos == std::wstring::npos?key.first.first.substr(path_length+1):key.first.first.substr(path_length+1,pos-path_length-1));
					}
				}
				list.unique();
			}


		};

			settings_map map;
			typedef std::map<std::wstring,std::set<std::wstring> > section_cache_type;
			section_cache_type section_cache_;


		public:


		OLDSettings(settings::settings_core *core, std::wstring context) : settings::SettingsInterfaceImpl(core, context), map(this) {
			get_logger()->debug(__FILE__, __LINE__, _T("Loading OLD: ") + context);
			map.read_map_file(core->find_file(_T("${exe-path}/old-settings.map"), _T("old-settings.map")));

			string_list list = get_keys(_T("includes"));
			BOOST_FOREACH(std::wstring key, list) {
				add_child(key);
			}
		}
		//////////////////////////////////////////////////////////////////////////
		/// Create a new settings interface of "this kind"
		///
		/// @param context the context to use
		/// @return the newly created settings interface
		///
		/// @author mickem
		virtual SettingsInterfaceImpl* create_new_context(std::wstring context) {
			return new OLDSettings(get_core(), context);
		}
		//////////////////////////////////////////////////////////////////////////
		/// Get a string value if it does not exist exception will be thrown
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @return the string value
		///
		/// @author mickem
		virtual std::wstring get_real_string(settings_core::key_path_type key) {
			key = map.key(key);
			return internal_get_value(key.first, key.second);
		}
#define UNLIKELY_STRING _T("$$$EMPTY_KEY$$$")

		std::wstring internal_get_value(std::wstring path, std::wstring key, int bufferSize = 1024) {
			if (!has_key_int(path, key))
				throw KeyNotFoundException(key);

			TCHAR* buffer = new TCHAR[bufferSize+2];
			if (buffer == NULL)
				throw settings_exception(_T("Out of memory error!"));
			int retVal = GetPrivateProfileString(path.c_str(), key.c_str(), _T(""), buffer, bufferSize, get_file_name().c_str());
			if (retVal == bufferSize-1) {
				delete [] buffer;
				return internal_get_value(path, key, bufferSize*10);
			}
			std::wstring ret = buffer;
			delete [] buffer;
			return ret;
		}

		//////////////////////////////////////////////////////////////////////////
		/// Get an integer value if it does not exist exception will be thrown
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @return the int value
		///
		/// @author mickem
		virtual int get_real_int(settings_core::key_path_type key) {
			std::wstring str = get_real_string(key);
			return strEx::stoi(str);
		}
		//////////////////////////////////////////////////////////////////////////
		/// Get a boolean value if it does not exist exception will be thrown
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @return the boolean value
		///
		/// @author mickem
		virtual bool get_real_bool(settings_core::key_path_type key) {
			std::wstring str = get_real_string(key);
			return SettingsInterfaceImpl::string_to_bool(str);
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


		std::set<std::wstring> internal_read_keys_from_section(std::wstring section, int bufferLength = 1024) {
			TCHAR* buffer = new TCHAR[bufferLength+1];
			if (buffer == NULL)
				throw settings_exception(_T("internal_read_keys_from_section:: Failed to allocate memory for buffer!"));
			unsigned int count = ::GetPrivateProfileSection(section.c_str(), buffer, bufferLength, get_file_name().c_str());
			if (count == bufferLength-2) {
				delete [] buffer;
				return internal_read_keys_from_section(section, bufferLength*10);
			}

			std::set<std::wstring> ret;
			unsigned int last = 0;
			for (unsigned int i=0;i<count;i++) {
				if (buffer[i] == '\0') {
					std::wstring s = &buffer[last];
					std::size_t p = s.find('=');
					ret.insert((p == std::wstring::npos)?s:s.substr(0,p));
					last = i+1;
				}
			}
			delete [] buffer;
			return ret;
		}

		bool has_key_int(std::wstring path, std::wstring key) {
			section_cache_type::const_iterator it = section_cache_.find(path);
			if (it == section_cache_.end()) {
				std::set<std::wstring> list = internal_read_keys_from_section(path);
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
				WritePrivateProfileString(key.first.c_str(), key.second.c_str(), value.get_string().c_str(), get_file_name().c_str());
			} catch (settings_exception e) {
				get_core()->get_logger()->err(__FILE__, __LINE__, std::wstring(_T("Failed to write key: ") + e.getError()));
			} catch (...) {
				get_core()->get_logger()->err(__FILE__, __LINE__, std::wstring(_T("Unknown filure when writing key: ") + key.first + _T(".") + key.second));
			}
		}

		virtual void set_real_path(std::wstring path) {
			// NOT Supported (and not needed) so silently ignored!
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
		virtual void get_real_sections(std::wstring path, string_list &list) {
			unsigned int path_length = path.length();
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
			TCHAR* buffer = new TCHAR[bufferLength+1];
			if (buffer == NULL)
				throw settings_exception(_T("getSections:: Failed to allocate memory for buffer!"));
			unsigned int count = ::GetPrivateProfileSectionNames(buffer, BUFF_LEN, get_file_name().c_str());
			if (count == bufferLength-2) {
				delete [] buffer;
				return int_read_sections(bufferLength*10);
			}
			unsigned int last = 0;
			for (unsigned int i=0;i<count;i++) {
				if (buffer[i] == '\0') {
					std::wstring s = &buffer[last];
					ret.push_back(s);
					last = i+1;
				}
			}
			delete [] buffer;
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
		virtual void get_real_keys(std::wstring path, string_list &list) {
			if (path.empty() || path == _T("/")) {
				get_core()->get_logger()->debug(__FILE__, __LINE__, std::wstring(_T("Loose leaves not supported: TODO")));
				return;
			}
			// @todo: this will NOT work for "nodes in paths"
			BOOST_FOREACH(settings_map::keys_key_type key, map.keys_) {
				if (path == key.first.first) {
					if (has_key_int(key.second.first, key.second.second))
						list.push_back(key.first.second);
				}
			}

			BOOST_FOREACH(settings_map::section_key_type key, map.sections_) {
				if (key.first == path) {
					section_cache_type::const_iterator it = section_cache_.find(key.second);
					if (it == section_cache_.end()) {
						std::set<std::wstring> list = internal_read_keys_from_section(key.second);
						section_cache_[path] = list;
						it = section_cache_.find(path);
					}
					list.insert(list.end(), (*it).second.begin(), (*it).second.end());
				}
			}

		}
	private:

		void int_read_section(std::wstring section, string_list &list, unsigned int bufferLength = BUFF_LEN) {
			TCHAR* buffer = new TCHAR[bufferLength+1];
			if (buffer == NULL)
				throw settings_exception(_T("getSections:: Failed to allocate memory for buffer!"));
			unsigned int count = GetPrivateProfileSection(section.c_str(), buffer, bufferLength, get_file_name().c_str());
			if (count == bufferLength-2) {
				delete [] buffer;
				int_read_section(section, list, bufferLength*10);
				return;
			}
			unsigned int last = 0;
			for (unsigned int i=0;i<count;i++) {
				if (buffer[i] == '\0') {
					std::wstring s = &buffer[last];
					std::size_t p = s.find('=');
					if (p == std::wstring::npos)
						list.push_back(s);
					else
						list.push_back(s.substr(0,p));
					last = i+1;
				}
			}
			delete [] buffer;
		}

		string_list int_read_section_from_inifile(std::wstring section, unsigned int bufferLength = BUFF_LEN) {
			TCHAR* buffer = new TCHAR[bufferLength+1];
			if (buffer == NULL)
				throw settings_exception(_T("getSections:: Failed to allocate memory for buffer!"));
			unsigned int count = GetPrivateProfileSection(section.c_str(), buffer, bufferLength, get_file_name().c_str());
			if (count == bufferLength-2) {
				delete [] buffer;
				return int_read_section_from_inifile(section, bufferLength*10);
			}
			unsigned int last = 0;
			string_list list;
			for (unsigned int i=0;i<count;i++) {
				if (buffer[i] == '\0') {
					std::wstring s = &buffer[last];
					std::size_t p = s.find('=');
					if (p == std::wstring::npos)
						list.push_back(s);
					else
						list.push_back(s.substr(0,p));
					last = i+1;
				}
			}
			delete [] buffer;
			return list;
		}


		inline std::wstring get_file_name() {
			if (filename_.empty()) {
				filename_ = get_file_from_context();
				//filename_ = (get_core()->get_base() / get_core()->get_boot_string(get_context(), _T("file"), _T("nsc.ini"))).string();
				get_core()->get_logger()->debug(__FILE__, __LINE__, _T("Reading old settings from: ") + filename_);
			}
			return filename_;
		}
		bool file_exists() {
			return boost::filesystem::is_regular_file(get_file_name());
		}
		virtual std::wstring get_info() {
			return _T("INI settings: (") + context_ + _T(", ") + get_file_name() + _T(")");
		}
	};
}
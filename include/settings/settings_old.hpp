#pragma once

#include <string>
#include <map>
#include <settings/settings_core.hpp>
#include <simpleini/SimpleIni.h>
#include <settings/macros.h>
#include <iostream>
#include <fstream>


//#define MAIN_MODULES_SECTION_OLD _T("modules")
//#define MAIN_SECTION_TITLE _T("Settings")
//#define MAIN_STRING_LENGTH _T("string_length")

namespace settings {
	class OLDSettings : public settings::SettingsInterfaceImpl {
		std::wstring filename_;
	public:
		OLDSettings(settings::settings_core *core, std::wstring context) : settings::SettingsInterfaceImpl(core, context) {
			std::wstring fname = core->find_file(_T("${exe-path}/old-settings.map"), _T("old-settings.map"));
			read_map_file(fname);
		}
		void read_map_file(std::wstring file) {
			get_logger()->debug(__FILE__, __LINE__, _T("Reading MAP file: ") + file);

			std::ifstream in(strEx::wstring_to_string(file).c_str());
			if(!in) {
				get_logger()->err(__FILE__, __LINE__, _T("Failed to read MAP file: ") + file);
				return;
			}
			in.exceptions(std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit);

			try{
				std::string tmp;
				while(true) {
					std::getline(in,tmp);
					parse_line(to_wstring(tmp));
				}
			}
			catch(std::ifstream::failure e){
				if(!in.eof())
					cerr << e.what() <<'\n';
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
				get_logger()->err(__FILE__, __LINE__, _T("Invalid syntax: ") + line);
				return;
			}
			std::pair<std::wstring,std::wstring> old_key = split_key(line.substr(0, pos));
			std::pair<std::wstring,std::wstring> new_key = split_key(line.substr(pos+1));
			if (old_key.second == _T("*") || old_key.second.empty()) {
				add_mapping(new_key.first, old_key.first);
				get_logger()->debug(__FILE__, __LINE__, _T("Adding: ") + old_key.first + _T(" >> ") + new_key.first);
			} else {
				add_mapping(new_key.first, new_key.second, old_key.first, old_key.second);
				get_logger()->debug(__FILE__, __LINE__, _T("Adding: ") + old_key.first + _T(":") + old_key.second + _T(" >> ") + new_key.first + _T(":") + new_key.second);
			}

		}
		std::pair<std::wstring,std::wstring> split_key(std::wstring key) {
			std::pair<std::wstring,std::wstring> ret;
			int pos = key.find_last_of('/');
			if (pos == -1)
				return std::pair<std::wstring,std::wstring>(key, _T(""));
			return std::pair<std::wstring,std::wstring>(key.substr(0, pos), key.substr(pos+1));
		}

		typedef std::map<std::wstring,std::wstring> path_map;
		typedef std::map<settings_core::key_path_type,settings_core::key_path_type> key_map;
		path_map sections_;
		key_map keys_;
		void add_mapping(std::wstring path_new, std::wstring path_old) {
			sections_[path_new] = path_old;
		}
		void add_mapping(std::wstring path_new, std::wstring key_new, std::wstring path_old, std::wstring key_old) {
			settings_core::key_path_type new_key(path_new, key_new);
			settings_core::key_path_type old_key(path_old, key_old);
			keys_[new_key] = old_key;
		}
		std::wstring map_path(std::wstring path_new) {
			path_map::iterator it = sections_.find(path_new);
			if (it == sections_.end())
				return path_new;
			//get_core()->get_logger()->debug(__FILE__, __LINE__, _T("Mapping: ") + path_new + _T(" to ") + (*it).second);
			return (*it).second;
		}
		settings_core::key_path_type map_key(settings_core::key_path_type new_key) {
			key_map::iterator it1 = keys_.find(new_key);
			if (it1 != keys_.end())
				return (*it1).second;
			path_map::iterator it2 = sections_.find(new_key.first);
			if (it2 != sections_.end())
				return settings_core::key_path_type((*it2).second, new_key.second);
			return new_key;
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
			key = map_key(key);
			get_core()->get_logger()->quick_debug(key.first + _T("//") + key.second);
			return internal_get_value(key.first, key.second.c_str());
		}
#define UNLIKELY_STRING _T("$$$EMPTY_KEY$$$")

		std::wstring internal_get_value(std::wstring path, std::wstring key, int bufferSize = 1024) {
			get_core()->get_logger()->quick_debug(path + _T("//") + key);
			TCHAR* buffer = new TCHAR[bufferSize+2];
			if (buffer == NULL)
				throw settings_exception(_T("Out of memmory error!"));
			int retVal = GetPrivateProfileString(path.c_str(), key.c_str(), UNLIKELY_STRING, buffer, bufferSize, get_file_name().c_str());
			if (retVal == bufferSize-1) {
				delete [] buffer;
				return internal_get_value(path, key, bufferSize*10);
			}
			std::wstring ret = buffer;
			delete [] buffer;
			if (ret != UNLIKELY_STRING)
				return ret;
			if (has_key_int(path, key)) {
				return _T("");
			}
			throw KeyNotFoundException(key);
			//return ret;
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
			return has_key_int(key.first, key.second);
		}

		bool has_key_int(std::wstring path, std::wstring key, int bufferLength=1024) {
			string_list ret;
			TCHAR* buffer = new TCHAR[bufferLength+1];
			if (buffer == NULL)
				throw settings_exception(_T("has_key_int:: Failed to allocate memory for buffer!"));
			std::wstring mapped = map_path(path);
			unsigned int count = ::GetPrivateProfileSection(mapped.c_str(), buffer, bufferLength-2, get_file_name().c_str());
			if (count == bufferLength-2) {
				delete [] buffer;
				return has_key_int(path, key, bufferLength*10);
			}

			unsigned int last = 0;
			for (unsigned int i=0;i<count;i++) {
				if (buffer[i] == '\0') {
					std::wstring s = &buffer[last];
					std::size_t p = s.find('=');
					if ((p == std::wstring::npos && s == key) || (s.substr(0,p) == key)) {
						delete [] buffer;
						return true;
					}
					last = i+1;
				}
			}
			delete [] buffer;
			return false;
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
				key = map_key(key);
				get_core()->get_logger()->quick_debug(key.first + _T("//") + key.second + _T("//") + value.get_string());
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
			get_core()->get_logger()->debug(__FILE__, __LINE__, std::wstring(_T("Get sections for: ")) + path);
			//string_list lst = get_mapped_sections(path);
			//list.insert(list.end(), lst.begin(), lst.end());
			/*
			string_list src = int_read_sections();
			for (string_list::const_iterator cit = src.begin(); cit != src.end(); ++cit) {
				std::wstring mapped = get_core()->reverse_map_path((*cit));
				if (path.empty() || path == _T("/")) {
					std::wstring::size_type pos = mapped.find(L'/', 1);
					if (pos != std::wstring::npos)
						mapped = mapped.substr(0,pos);
					get_core()->get_logger()->debug(__FILE__, __LINE__, std::wstring(_T("Found: ")) + mapped);
					list.push_back(mapped);
				} else if (mapped.length() > path.length() && mapped == path.substr(0, path.length())) {
					get_core()->get_logger()->debug(__FILE__, __LINE__, std::wstring(_T("Found: FUCKED")) + mapped);
				}
			}
			*/
			//list.insert(list.end(), src.begin(), src.end());
			/*
			CSimpleIni::TNamesDepend lst;
			ini.GetAllSections(lst);
			if (path.empty()) {
				for (CSimpleIni::TNamesDepend::const_iterator cit = lst.begin(); cit != lst.end(); ++cit) {
					std::wstring mapped = get_core()->reverse_map_path((*cit).pItem);
					if (mapped.length() > 1) {
						std::wstring::size_type pos = mapped.find(L'/', 1);
						if (pos != std::wstring::npos)
							mapped = mapped.substr(0,pos);
					}
					list.push_back(mapped);
				}
			} else {
				for (CSimpleIni::TNamesDepend::const_iterator cit = lst.begin(); cit != lst.end(); ++cit) {
					std::wstring mapped = get_core()->reverse_map_path((*cit).pItem);
					get_core()->get_logger()->debug(__FILE__, __LINE__, std::wstring(_T("Looking for: ")) + mapped + _T(": ") + mapped);
					std::wstring::size_type mapped_len = mapped.length();
					std::wstring::size_type path_len = path.length();
					if (mapped_len > path_len+1 && mapped.substr(0,path_len) == path) {
						std::wstring::size_type pos = mapped.find(L'/', path_len+1);
						if (pos == std::wstring::npos)
							mapped = mapped.substr(path_len+1);
						else
							mapped = mapped.substr(path_len+1, pos-path_len-1);
						list.push_back(mapped);
					}
				}
			}
			*/
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
			std::wstring mapped_path = map_path(path);
			int_read_section(mapped_path, list);
			/*
			settings::settings_core::mapped_key_list_type mapped_keys = get_core()->find_maped_keys(path);
			for (settings::settings_core::mapped_key_list_type::const_iterator cit = mapped_keys.begin(); cit != mapped_keys.end(); ++cit) {
				if (has_key((*cit).dst.first, (*cit).dst.second))
					list.push_back((*cit).src.second);
			}
			*/
		}
// 		virtual settings_core::key_type get_key_type(std::wstring path, std::wstring key) {
// 			return settings_core::key_string;
// 		}
	private:
		bool has_key(std::wstring section, std::wstring key) {
			TCHAR* buffer = new TCHAR[1024];
			GetPrivateProfileString(section.c_str(), key.c_str(), UNLIKELY_STRING, buffer, 1023, get_file_name().c_str());
			std::wstring ret = buffer;
			delete [] buffer;
			return ret != UNLIKELY_STRING;
		}
		void int_read_section(std::wstring section, string_list &list, unsigned int bufferLength = BUFF_LEN) {
			//get_core()->get_logger()->debug(__FILE__, __LINE__, _T("Reading (OLD) section: ") + section);
			// @TODO this is not correct!
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
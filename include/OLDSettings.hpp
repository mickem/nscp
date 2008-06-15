#pragma once

#include <string>
#include <map>
#include <Settings.h>
#include <simpleini/SimpleIni.h>

namespace Settings {
	class OLDSettings : public Settings::SettingsInterfaceImpl {
		std::wstring filename_;
	public:
		OLDSettings(Settings::SettingsCore *core, std::wstring context) :Settings::SettingsInterfaceImpl(core, context) {}
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
		virtual std::wstring get_real_string(SettingsCore::key_path_type key) {
			key = get_core()->map_key(key);
			get_core()->get_logger()->quick_debug(key.first + _T("//") + key.second);
			return internal_get_value(key.first, key.second.c_str());
		}
#define UNLIKELY_STRING _T("$$$EMPTY_KEY$$$")

		std::wstring internal_get_value(std::wstring path, std::wstring key, int bufferSize = 1024) {
			TCHAR* buffer = new TCHAR[bufferSize+2];
			if (buffer == NULL)
				throw SettingsException(_T("Out of memmory error!"));
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
		virtual int get_real_int(SettingsCore::key_path_type key) {
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
		virtual bool get_real_bool(SettingsCore::key_path_type key) {
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
		virtual bool has_real_key(SettingsCore::key_path_type key) {
			return has_key_int(key.first, key.second);
		}

		bool has_key_int(std::wstring path, std::wstring key, int bufferLength=1024) {
			string_list ret;
			TCHAR* buffer = new TCHAR[bufferLength+1];
			if (buffer == NULL)
				throw SettingsException(_T("has_key_int:: Failed to allocate memory for buffer!"));
			std::wstring mapped = get_core()->map_path(path);
			unsigned int count = ::GetPrivateProfileSection(mapped.c_str(), buffer, BUFF_LEN, get_file_name().c_str());
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
		/// Get the type this settings store represent.
		///
		/// @return the type of settings store
		///
		/// @author mickem
		virtual SettingsCore::settings_type get_type() {
			return SettingsCore::old_ini_file;
		}
		//////////////////////////////////////////////////////////////////////////
		/// Is this the active settings store
		///
		/// @return
		///
		/// @author mickem
		virtual bool is_active() {
			return true;
		}
		//////////////////////////////////////////////////////////////////////////
		/// Write a value to the resulting context.
		///
		/// @param key The key to write to
		/// @param value The value to write
		///
		/// @author mickem
		virtual void set_real_value(SettingsCore::key_path_type key, conainer value) {
			try {
				key = get_core()->map_key(key);
				get_core()->get_logger()->quick_debug(key.first + _T("//") + key.second + _T("//") + value.get_string());
				WritePrivateProfileString(key.first.c_str(), key.second.c_str(), value.get_string().c_str(), get_file_name().c_str());
			} catch (SettingsException e) {
				get_core()->get_logger()->err(__FILEW__, __LINE__, std::wstring(_T("Failed to write key: ") + e.getError()));
			} catch (...) {
				get_core()->get_logger()->err(__FILEW__, __LINE__, std::wstring(_T("Unknown filure when writing key: ") + key.first + _T(".") + key.second));
			}
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
			get_core()->get_logger()->debug(__FILEW__, __LINE__, std::wstring(_T("Get sections for: ")) + path);
			string_list lst = get_core()->get_mapped_sections(path);
			list.insert(list.end(), lst.begin(), lst.end());
			/*
			string_list src = int_read_sections();
			for (string_list::const_iterator cit = src.begin(); cit != src.end(); ++cit) {
				std::wstring mapped = get_core()->reverse_map_path((*cit));
				if (path.empty() || path == _T("/")) {
					std::wstring::size_type pos = mapped.find(L'/', 1);
					if (pos != std::wstring::npos)
						mapped = mapped.substr(0,pos);
					get_core()->get_logger()->debug(__FILEW__, __LINE__, std::wstring(_T("Found: ")) + mapped);
					list.push_back(mapped);
				} else if (mapped.length() > path.length() && mapped == path.substr(0, path.length())) {
					get_core()->get_logger()->debug(__FILEW__, __LINE__, std::wstring(_T("Found: FUCKED")) + mapped);
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
					get_core()->get_logger()->debug(__FILEW__, __LINE__, std::wstring(_T("Looking for: ")) + mapped + _T(": ") + mapped);
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
				throw SettingsException(_T("getSections:: Failed to allocate memory for buffer!"));
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
			std::wstring mapped_path = get_core()->reverse_map_path(path);
			int_read_section(path, list);
			Settings::SettingsCore::mapped_key_list_type mapped_keys = get_core()->find_maped_keys(path);
			for (Settings::SettingsCore::mapped_key_list_type::const_iterator cit = mapped_keys.begin(); cit != mapped_keys.end(); ++cit) {
				if (has_key((*cit).dst.first, (*cit).dst.second))
					list.push_back((*cit).src.second);
			}
		}
		virtual SettingsCore::key_type get_key_type(std::wstring path, std::wstring key) {
			return SettingsCore::key_string;
		}
	private:
		bool has_key(std::wstring section, std::wstring key) {
			TCHAR* buffer = new TCHAR[1024];
			GetPrivateProfileString(section.c_str(), key.c_str(), UNLIKELY_STRING, buffer, 1023, get_file_name().c_str());
			std::wstring ret = buffer;
			delete [] buffer;
			return ret != UNLIKELY_STRING;
		}
		void int_read_section(std::wstring section, string_list &list, unsigned int bufferLength = BUFF_LEN) {
			std::wstring mapped = get_core()->map_path(section);
			TCHAR* buffer = new TCHAR[bufferLength+1];
			if (buffer == NULL)
				throw SettingsException(_T("getSections:: Failed to allocate memory for buffer!"));
			unsigned int count = GetPrivateProfileSection(mapped.c_str(), buffer, bufferLength, get_file_name().c_str());
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
				filename_ = get_core()->get_base() + _T("\\") + get_core()->get_boot_string(get_context(), _T("file"), _T("nsc.ini"));
				get_core()->get_logger()->debug(__FILEW__, __LINE__, _T("Reading old settings from: ") + filename_);
			}
			return filename_;
		}
		bool file_exists() {
			std::wstring filename = get_file_name();
			FILE * fp = NULL;
			bool found = false;
#if __STDC_WANT_SECURE_LIB__
			if (_wfopen_s(&fp, filename.c_str(), L"rb") != 0)
				return false;
#else
			fp = _wfopen(filename.c_str(), L"rb");
#endif
			if (!fp)
				return false;
			fclose(fp);
			return true;
		}
	};
}
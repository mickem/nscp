#pragma once

#include <string>
#include <map>

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

#include <settings/Settings.h>
#include <simpleini/simpleini.h>
#include <error.hpp>

namespace Settings {
	class INISettings : public Settings::SettingsInterfaceImpl {
	private:
		boost::filesystem::wpath filename_;
		bool is_loaded_;
		CSimpleIni ini;

	public:
		INISettings(Settings::SettingsCore *core, std::wstring context) : ini(false, false, false), is_loaded_(false), Settings::SettingsInterfaceImpl(core, context) {}
		//////////////////////////////////////////////////////////////////////////
		/// Create a new settings interface of "this kind"
		///
		/// @param context the context to use
		/// @return the newly created settings interface
		///
		/// @author mickem
		virtual SettingsInterfaceImpl* create_new_context(std::wstring context) {
			return new INISettings(get_core(), context);
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
			load_data();
			const wchar_t *val = ini.GetValue(key.first.c_str(), key.second.c_str(), NULL);
			if (val == NULL)
				throw KeyNotFoundException(key);
			return val;
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
			return ini.GetValue(key.first.c_str(), key.second.c_str()) != NULL;
		}
		//////////////////////////////////////////////////////////////////////////
		/// Get the type this settings store represent.
		///
		/// @return the type of settings store
		///
		/// @author mickem
		virtual SettingsCore::settings_type get_type() {
			return SettingsCore::ini_file;
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
				const SettingsCore::key_description desc = get_core()->get_registred_key(key.first, key.second);
				std::wstring comment = _T("; ");
				if (!desc.title.empty())
					comment += desc.title + _T(" - ");
				if (!desc.description.empty())
					comment += desc.description;
				strEx::replace(comment, _T("\n"), _T(" "));
				get_core()->get_logger()->quick_debug(_T("saving: ") + key.first + _T("//") + key.second);
				
				ini.Delete(key.first.c_str(), key.second.c_str());
				ini.SetValue(key.first.c_str(), key.second.c_str(), value.get_string().c_str(), comment.c_str());
			} catch (KeyNotFoundException e) {
				ini.SetValue(key.first.c_str(), key.second.c_str(), value.get_string().c_str(), _T("; Undocumented key"));
			} catch (SettingsException e) {
				get_core()->get_logger()->err(__FILEW__, __LINE__, std::wstring(_T("Failed to write key: ") + e.getError()));
			} catch (...) {
				get_core()->get_logger()->err(__FILEW__, __LINE__, std::wstring(_T("Unknown filure when writing key: ") + key.first + _T(".") + key.second));
			}
		}

		virtual void set_real_path(std::wstring path) {
			try {
				get_core()->get_logger()->quick_debug(_T("Setting path: ") + path);
				const SettingsCore::path_description desc = get_core()->get_registred_path(path);
				if (!desc.description.empty()) {
					std::wstring comment = _T("; ") + desc.description;
					ini.SetValue(path.c_str(), NULL, NULL, comment.c_str());
				}
			} catch (KeyNotFoundException e) {
				ini.SetValue(path.c_str(), NULL, NULL, _T("; Undocumented section"));
			} catch (SettingsException e) {
				get_core()->get_logger()->err(__FILEW__, __LINE__, std::wstring(_T("Failed to write section: ") + e.getError()));
			} catch (...) {
				get_core()->get_logger()->err(__FILEW__, __LINE__, std::wstring(_T("Unknown filure when writing section: ") + path));
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
			CSimpleIni::TNamesDepend lst;
			ini.GetAllSections(lst);
			if (path.empty()) {
				for (CSimpleIni::TNamesDepend::const_iterator cit = lst.begin(); cit != lst.end(); ++cit) {
					std::wstring mapped = (*cit).pItem;
					if (mapped.length() > 1) {
						std::wstring::size_type pos = mapped.find(L'/', 1);
						if (pos != std::wstring::npos)
							mapped = mapped.substr(0,pos);
					}
					list.push_back(mapped);
				}
			} else {
				for (CSimpleIni::TNamesDepend::const_iterator cit = lst.begin(); cit != lst.end(); ++cit) {
					std::wstring mapped = (*cit).pItem;
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
			load_data();
			get_core()->get_logger()->debug(__FILEW__, __LINE__, std::wstring(_T("Looking for: ")) + path);
			CSimpleIni::TNamesDepend lst;
			ini.GetAllKeys(path.c_str(), lst);
			for (CSimpleIni::TNamesDepend::const_iterator cit = lst.begin(); cit != lst.end(); ++cit) {
				list.push_back((*cit).pItem);
			}
		}
		//////////////////////////////////////////////////////////////////////////
		/// Save the settings store
		///
		/// @author mickem
		virtual void save() {
			SettingsInterfaceImpl::save();
			SI_Error rc = ini.SaveFile(get_file_name().string().c_str());
			if (rc < 0)
				throw_SI_error(rc, _T("Failed to save file"));
		}
		virtual SettingsCore::key_type get_key_type(std::wstring path, std::wstring key) {
			return SettingsCore::key_string;
		}
	private:
		void load_data() {
			if (is_loaded_)
				return;
			if (!file_exists()) {
				is_loaded_ = true;
				return;
			}
			SI_Error rc = ini.LoadFile(get_file_name().string().c_str());
			if (rc < 0)
				throw_SI_error(rc, _T("Failed to load file"));
			is_loaded_ = true;
		}
		void throw_SI_error(SI_Error err, std::wstring msg) {
			std::wstring error_str = _T("unknown error");
			if (err == SI_NOMEM)
				error_str = _T("Out of memmory");
			if (err == SI_FAIL)
				error_str = _T("General failure");
			if (err == SI_FILE)
				error_str = _T("I/O error: ") + error::lookup::last_error();
			throw SettingsException(msg + _T(": ") + get_context() + _T(" - ") + error_str);
		}
		boost::filesystem::wpath get_file_name() {
			if (filename_.empty()) {
				filename_ = get_core()->get_base() / boost::filesystem::wpath(get_core()->get_boot_string(get_context(), _T("file"), _T("nsclient.ini")));
				get_core()->get_logger()->debug(__FILEW__, __LINE__, _T("Reading INI settings from: ") + filename_.string());
			}
			return filename_;
		}
		bool file_exists() {
			return boost::filesystem::is_regular(get_file_name());
		}
	};
}

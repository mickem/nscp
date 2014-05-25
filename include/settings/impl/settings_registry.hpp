#pragma once

#include <boost/algorithm/string.hpp>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>

#include <settings/settings_core.hpp>
#include <error.hpp>
#include <settings/macros.h>

#include <handle.hpp>
#include <buffer.hpp>

#define BUFF_LEN 4096


namespace settings {

	struct registry_closer {
		static void close(HKEY hKey) {
			RegCloseKey(hKey);
		}
	};
	typedef hlp::handle<HKEY, registry_closer> reg_handle;
	typedef hlp::buffer<wchar_t, const BYTE*> reg_buffer;
	class REGSettings : public settings::SettingsInterfaceImpl {
	private:
		struct reg_key {
			HKEY hKey;
			std::wstring path;

			static reg_key from_context(std::string context) {
				net::url url = net::parse(context);
				std::string file = url.path;
				boost::replace_all(file, "/", "\\");
				HKEY hKey;
				std::wstring path;
				if (url.host == "HKEY_LOCAL_MACHINE" || url.host == "LOCAL_MACHINE" || url.host == "HKLM")
					hKey = HKEY_LOCAL_MACHINE;
				else if (url.host == "HKEY_CURRENT_USER" || url.host == "CURRENT_USER" || url.host == "HKCU")
					hKey = HKEY_CURRENT_USER;
				else
					hKey = NS_HKEY_ROOT;
				if (!file.empty()) {
					if (file[0] == '\\')
						file = file.substr(1);
					path = utf8::cvt<std::wstring>(file);
				} else
					path = NS_REG_ROOT;
				return reg_key(hKey, path);
			}
			reg_key(HKEY hKey, std::wstring path) : hKey(hKey), path(path) {}
			std::string to_string() const {
				return utf8::cvt<std::string>(path);
			}
			reg_key get_subkey(std::wstring sub_path) const {
				return reg_key(hKey, path + boost::replace_all_copy(sub_path, _T("/"), _T("\\")));
			}
			reg_key get_subkey(std::string sub_path) const {
				return get_subkey(utf8::cvt<std::wstring>(sub_path));
			}
		};



		struct open_reg_key {
			reg_handle hTemp;
			const reg_key &source;
			open_reg_key(const reg_key &source) : source(source) {
				open();
			}
			void open() {
				DWORD err = RegCreateKeyEx(source.hKey, source.path.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, hTemp.ref(), NULL);
				if (err != ERROR_SUCCESS) {
					throw settings_exception("Failed to open key " + source.to_string() + ": " + error::lookup::last_error(err));
				}
			}
			void ensure() {
				open();
			}
			HKEY operator*() {
				return hTemp;
			}
			void setValueEx(const std::string &key, DWORD type, const BYTE *lpData, DWORD size) const {
				std::wstring wkey = utf8::cvt<std::wstring>(key);
				DWORD err = RegSetValueExW(hTemp.get(), wkey.c_str(), 0, type, lpData, size);
				if (err != ERROR_SUCCESS) {
					throw settings_exception("Failed to write string " + source.to_string() + "." + key + ": " + error::lookup::last_error(err));
				}
			}
			inline void setValueEx(const std::string &key, DWORD type, const reg_buffer &buffer) const {
				return setValueEx(key, type, buffer.get(), buffer.size_in_bytes());
			}
			inline void setValueEx(const std::string &key, DWORD type, const DWORD value) const {
				return setValueEx(key, type, reinterpret_cast<const BYTE*>(&value), sizeof(DWORD));
			}


		};

		reg_key root;


	public:
		REGSettings(settings::settings_core *core, std::string context) : settings::SettingsInterfaceImpl(core, context), root(reg_key::from_context(context)) {
			
		}


		virtual ~REGSettings(void) {}

		//////////////////////////////////////////////////////////////////////////
		/// Create a new settings interface of "this kind"
		///
		/// @param context the context to use
		/// @return the newly created settings interface
		///
		/// @author mickem
		virtual SettingsInterfaceImpl* create_new_context(std::string context) {
			return new REGSettings(get_core(), context);
		}
		//////////////////////////////////////////////////////////////////////////
		/// Get a string value if it does not exist exception will be thrown
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @return the string value
		///
		/// @author mickem
		virtual std::string get_real_string(settings_core::key_path_type key) {
			return getString_(get_reg_key(key.first), key.second);
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
			std::string str = get_real_string(key);
			return strEx::s::stox<int>(str);
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
			std::string str = get_real_string(key);
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
			if (value.type == settings_core::key_string) {
				setString_(get_reg_key(key), key.second, value.get_string());
			} else if (value.type == settings_core::key_integer) {
				setInt_(get_reg_key(key), key.second, value.get_int());
			} else if (value.type == settings_core::key_bool) {
				setInt_(get_reg_key(key), key.second, value.get_bool()?1:0);
			} else {
				throw settings_exception("Invalid settings type.");
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
			getSubKeys_(get_reg_key(path), list);
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
			getValues_(get_reg_key(path), list);
		}
	private:
		reg_key get_reg_key(const settings_core::key_path_type key) const {
			return root.get_subkey(key.first);
		}
		reg_key get_reg_key(const std::string path) const {
			return root.get_subkey(path);
		}


		static void setString_(const reg_key &path, std::string key, std::string value) {
			open_reg_key open_key(path);
			std::wstring wvalue = utf8::cvt<std::wstring>(value);
			reg_buffer buffer(wvalue.size()+10, wvalue.c_str());
			open_key.setValueEx(key, REG_SZ, buffer);
		}

		static void setInt_(const reg_key &path, std::string key, DWORD value) {
			open_reg_key open_key(path);
			open_key.setValueEx(key, REG_DWORD, value);
		}

		static std::string getString_(reg_key path, std::string key) {
			reg_handle hTemp;
			if (RegOpenKeyEx(path.hKey, path.path.c_str(), 0, KEY_QUERY_VALUE, hTemp.ref()) != ERROR_SUCCESS) 
				throw KeyNotFoundException("Failed to open key: " + path.to_string() + ": " + error::lookup::last_error());
			DWORD type;
			const DWORD data_length = 2048;
			DWORD cbData = data_length;
			hlp::buffer<BYTE> bData(cbData);
			LONG lRet = RegQueryValueEx(hTemp, utf8::cvt<std::wstring>(key).c_str(), NULL, &type, bData.get(), &cbData);
			if (lRet == ERROR_SUCCESS) {
				if (type == REG_SZ || type == REG_EXPAND_SZ) {
					if (cbData == 0)
						return "";
					if (cbData < data_length-1) {
						bData[cbData] = 0;
						return utf8::cvt<std::string>(std::wstring(reinterpret_cast<TCHAR*>(bData.get())));
					}
					throw settings_exception("String to long: " + path.to_string());
				} else if (type == REG_DWORD) {
					DWORD dw = *(reinterpret_cast<DWORD*>(bData.get()));
					return strEx::s::xtos(dw);
				}
				throw settings_exception("Unsupported key type: " + path.to_string());
			} else if (lRet == ERROR_FILE_NOT_FOUND)
				throw KeyNotFoundException("Key not found: " + path.to_string());
			throw settings_exception("Failed to open key: " + path.to_string() + ": " + error::lookup::last_error(lRet));
		}
		static DWORD getInt_(HKEY hKey, LPCTSTR lpszPath, LPCTSTR lpszKey, DWORD def) {
			DWORD ret = def;
			LONG bRet;
			reg_handle hTemp;
			if ((bRet = RegOpenKeyEx(hKey, lpszPath, 0, KEY_READ, hTemp.ref())) != ERROR_SUCCESS) {
				return def;
			}
			DWORD type;
			DWORD cbData = sizeof(DWORD);
			DWORD buffer;
			bRet = RegQueryValueEx(hTemp, lpszKey, NULL, &type, reinterpret_cast<LPBYTE>(&buffer), &cbData );
			if (type != REG_DWORD)
				throw settings_exception("Unsupported key type: ");
			if (bRet == ERROR_SUCCESS) {
				ret = buffer;
			}
			return ret;
		}
		static void getValues_(reg_key path, string_list &list) {
			LONG bRet;
			reg_handle hTemp;
			if ((bRet = RegOpenKeyEx(path.hKey, path.path.c_str(), 0, KEY_READ, hTemp.ref())) != ERROR_SUCCESS)
				return;
			DWORD cValues=0;
			DWORD cMaxValLen=0;
			// Get the class name and the value count. 
			bRet = RegQueryInfoKey(hTemp,NULL,NULL,NULL,NULL,NULL,NULL,&cValues,&cMaxValLen,NULL,NULL,NULL);
			cMaxValLen++;
			if ((bRet == ERROR_SUCCESS)&&(cValues>0)) {
				hlp::buffer<wchar_t> lpValueName(cMaxValLen+1);
				for (unsigned int i=0; i<cValues; i++) {
					DWORD len = cMaxValLen;
					bRet = RegEnumValue(hTemp, i, lpValueName.get(), &len, NULL, NULL, NULL, NULL);
					if (bRet != ERROR_SUCCESS)
						throw settings_exception("Failed to enumerate: " + path.to_string() + ": " + error::lookup::last_error());
					list.push_back(utf8::cvt<std::string>(std::wstring(lpValueName)));
				}
			}
		}
		static bool has_key(reg_key path) {
			reg_handle hTemp;
			DWORD status = RegOpenKeyEx(path.hKey, path.path.c_str(), 0, KEY_READ, hTemp.ref());
			return status == ERROR_SUCCESS;
		}
		static void getSubKeys_(reg_key path, string_list &list) {
			LONG bRet;
			reg_handle hTemp;
			if ((bRet = RegOpenKeyEx(path.hKey, path.path.c_str(), 0, KEY_READ, hTemp.ref())) != ERROR_SUCCESS) 
				return;
			DWORD cSubKeys=0;
			DWORD cMaxKeyLen;
			// Get the class name and the value count. 
			bRet = RegQueryInfoKey(hTemp,NULL,NULL,NULL,&cSubKeys,&cMaxKeyLen,NULL,NULL,NULL,NULL,NULL,NULL);
			cMaxKeyLen++;
			if ((bRet == ERROR_SUCCESS)&&(cSubKeys>0)) {
				hlp::buffer<wchar_t> lpValueName(cMaxKeyLen+1);
				for (unsigned int i=0; i<cSubKeys; i++) {
					DWORD len = cMaxKeyLen;
					bRet = RegEnumKey(hTemp, i, lpValueName, len);
					if (bRet != ERROR_SUCCESS)
						throw settings_exception("Failed to enumerate: " + path.to_string() + ": " + error::lookup::last_error());
					list.push_back(utf8::cvt<std::string>(std::wstring(lpValueName)));
				}
			}
		}
		settings::error_list validate() {
			settings::error_list ret;
			return ret;
		}

		virtual std::string get_info() {
			return "Registry settings: (" + context_ + ",TODO)";
		}
public:
		static bool context_exists(settings::settings_core *, std::string key) {
			return has_key(reg_key::from_context(key));
		}
		virtual void real_clear_cache() {}
		void ensure_exists() {
			open_reg_key open_key(root);
			open_key.ensure();
		}


	};
}

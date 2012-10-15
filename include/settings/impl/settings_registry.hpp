#pragma once

#include <string>
#include <windows.h>
#include <settings/settings_core.hpp>
#include <error.hpp>
#include <settings/macros.h>
#define BUFF_LEN 4096


namespace settings {
	class REGSettings : public settings::SettingsInterfaceImpl {
	private:
		struct reg_key {
			HKEY hKey;
			std::wstring path;
			operator std::wstring () {
				return path;
			}
			std::wstring to_string() {
				return path;
			}
		};


		public:
		REGSettings(settings::settings_core *core, std::wstring context) : settings::SettingsInterfaceImpl(core, context) {}

		virtual ~REGSettings(void) {}

		//////////////////////////////////////////////////////////////////////////
		/// Create a new settings interface of "this kind"
		///
		/// @param context the context to use
		/// @return the newly created settings interface
		///
		/// @author mickem
		virtual SettingsInterfaceImpl* create_new_context(std::wstring context) {
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
		virtual std::wstring get_real_string(settings_core::key_path_type key) {
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
			throw KeyNotFoundException(key);
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
			throw KeyNotFoundException(key);
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
				throw settings_exception(_T("Invalid settings type."));
			}
		}
		virtual void set_real_path(std::wstring path) {
			// NOT Supported (and not needed) so silently ignored!
		}

		virtual void remove_real_value(settings_core::key_path_type key) {
			// NOT Supported
		}
		virtual void remove_real_path(std::wstring path) {
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
		virtual void get_real_sections(std::wstring path, string_list &list) {
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
		virtual void get_real_keys(std::wstring path, string_list &list) {
			getValues_(get_reg_key(path), list);
		}
	// 	virtual settings_core::key_type get_key_type(std::wstring path, std::wstring key) {
	// 		return SettingsInterfaceImpl::get_key_type(path, key);
	// 	}
	private:
		reg_key get_reg_key(settings_core::key_path_type key) {
			return get_reg_key(key.first);
		}
		reg_key get_reg_key(std::wstring path) {
			reg_key ret;
			strEx::replace(path, _T("/"), _T("\\"));
			ret.path = NS_REG_ROOT;
			if (path[0] != '\\')
				ret.path += _T("\\");
			ret.path += path;
			ret.hKey = NS_HKEY_ROOT;
			return ret;
		}
		struct reg_buffer {
			wchar_t *buf;
			std::wstring::size_type len;
			reg_buffer(std::wstring::size_type len) : buf(new wchar_t[len]), len(len) {}
			reg_buffer(std::wstring str) : buf(new wchar_t[str.length()]), len(str.length()) {
				copy_from(str);
			}
			~reg_buffer() {
				delete [] buf;
			}
			TCHAR* operator* () {
				return buf;
			}
			TCHAR& operator[] (int i) {
				return buf[i];
			}
			int str_len() {
				return (wcslen(buf)+1)*sizeof(wchar_t);
			}
			void copy_from(std::wstring &src) {
				if (src.length() > len)
					resize(src.length()+1);
				wcsncpy(buf, src.c_str(), src.length()+1);
				buf[src.length()] = 0;
			}
			void resize(unsigned int len) {
				wchar_t *tmp = buf;
				buf = new wchar_t[len+1];
				delete [] tmp;
			}
		};

		struct tmp_reg_key {
			HKEY hTemp;
			tmp_reg_key(HKEY hKey, std::wstring path) : hTemp(NULL) {
				DWORD err = RegCreateKeyEx(hKey, path.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hTemp, NULL);
				if (err != ERROR_SUCCESS) {
					throw settings_exception(_T("Failed to open key: ") + path + _T(" (") + error::format::from_system(err) + _T(")"));
				}
			}
			HKEY operator*() {
				return hTemp;
			}
			~tmp_reg_key() {
				if (hTemp)
					RegCloseKey(hTemp);
			}

		};

		static void setString_(reg_key path, std::wstring key, std::wstring value) {
			tmp_reg_key hTemp(path.hKey, path.path);
			reg_buffer buffer(value);
			DWORD err = RegSetValueExW(*hTemp, key.c_str(), 0, REG_EXPAND_SZ, reinterpret_cast<LPBYTE>(*buffer), buffer.str_len());
			if (err != ERROR_SUCCESS) {
				throw settings_exception(_T("Failed to write string: ") + path.to_string() + _T(".") + key + _T(" (") + error::format::from_system(err) + _T(")"));
			}
		}

		static void setInt_(reg_key path, std::wstring key, DWORD value) {
			tmp_reg_key hTemp(path.hKey, path.path);
			DWORD err = RegSetValueEx(*hTemp, key.c_str(), 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(DWORD));
			if (err != ERROR_SUCCESS) {
				throw settings_exception(_T("Failed to int string: ") + path.to_string() + _T(".") + key + _T(" (") + error::format::from_system(err) + _T(")"));
			}
		}

		static std::wstring getString_(reg_key path, std::wstring key) {
			HKEY hTemp;
			if (RegOpenKeyEx(path.hKey, path.path.c_str(), 0, KEY_QUERY_VALUE, &hTemp) != ERROR_SUCCESS) 
				throw KeyNotFoundException(_T("Failed to open key: ") + path.to_string() + _T(": ") + error::lookup::last_error());
			DWORD type;
			const DWORD data_length = 2048;
			DWORD cbData = data_length;
			BYTE *bData = new BYTE[cbData];
			LONG lRet = RegQueryValueEx(hTemp, key.c_str(), NULL, &type, bData, &cbData);
			RegCloseKey(hTemp);
			if (lRet == ERROR_SUCCESS) {
				if (type == REG_SZ) {
					if (cbData == 0) {
						delete [] bData;
						return _T("");
					}
					if (cbData < data_length-1) {
						bData[cbData] = 0;
						const TCHAR *ptr = reinterpret_cast<TCHAR*>(bData);
						std::wstring ret = ptr;
						delete [] bData;
						std::wcout << _T("read: ") << ret << std::endl;
						return ret;
					}
					throw settings_exception(_T("String to long: ") + path.to_string());
				} else if (type == REG_DWORD) {
					DWORD dw = *(reinterpret_cast<DWORD*>(bData));
					return strEx::itos(dw);
				}
				throw settings_exception(_T("Unsupported key type: ") + path.to_string());
			} else if (lRet == ERROR_FILE_NOT_FOUND)
				throw KeyNotFoundException(_T("Key not found: ") + path.to_string());
			throw settings_exception(_T("Failed to open key: ") + path.to_string() + _T(": ") + error::lookup::last_error(lRet));
		}
		static DWORD getInt_(HKEY hKey, LPCTSTR lpszPath, LPCTSTR lpszKey, DWORD def) {
			DWORD ret = def;
			LONG bRet;
			HKEY hTemp;
			if ((bRet = RegOpenKeyEx(hKey, lpszPath, 0, KEY_READ, &hTemp)) != ERROR_SUCCESS) {
				return def;
			}
			DWORD type;
			DWORD cbData = sizeof(DWORD);
			DWORD buffer;
			//BYTE *bData = new BYTE[cbData+1];
			bRet = RegQueryValueEx(hTemp, lpszKey, NULL, &type, reinterpret_cast<LPBYTE>(&buffer), &cbData );
			if (type != REG_DWORD) {
				bRet = -1;
			}
			RegCloseKey(hTemp);
			if (bRet == ERROR_SUCCESS) {
				ret = buffer;
			}
			//delete [] bData;
			return ret;
		}
		static void getValues_(reg_key path, string_list &list) {
			LONG bRet;
			HKEY hTemp;
			if ((bRet = RegOpenKeyEx(path.hKey, path.path.c_str(), 0, KEY_READ, &hTemp)) != ERROR_SUCCESS)
				return;
			DWORD cValues=0;
			DWORD cMaxValLen=0;
			// Get the class name and the value count. 
			bRet = RegQueryInfoKey(hTemp,NULL,NULL,NULL,NULL,NULL,NULL,&cValues,&cMaxValLen,NULL,NULL,NULL);
			cMaxValLen++;
			if ((bRet == ERROR_SUCCESS)&&(cValues>0)) {
				TCHAR *lpValueName = new TCHAR[cMaxValLen+1];
				for (unsigned int i=0; i<cValues; i++) {
					DWORD len = cMaxValLen;
					bRet = RegEnumValue(hTemp, i, lpValueName, &len, NULL, NULL, NULL, NULL);
					if (bRet != ERROR_SUCCESS) {
						delete [] lpValueName;
						throw settings_exception(_T("Failed to enumerate: ") + path.to_string() + _T(": ") + error::lookup::last_error());
					}
					list.push_back(std::wstring(lpValueName));
				}
				delete [] lpValueName;
			}
		}
		static void getSubKeys_(reg_key path, string_list &list) {
			LONG bRet;
			HKEY hTemp;
			if ((bRet = RegOpenKeyEx(path.hKey, path.path.c_str(), 0, KEY_READ, &hTemp)) != ERROR_SUCCESS) 
				return;
			DWORD cSubKeys=0;
			DWORD cMaxKeyLen;
			// Get the class name and the value count. 
			bRet = RegQueryInfoKey(hTemp,NULL,NULL,NULL,&cSubKeys,&cMaxKeyLen,NULL,NULL,NULL,NULL,NULL,NULL);
			cMaxKeyLen++;
			if ((bRet == ERROR_SUCCESS)&&(cSubKeys>0)) {
				TCHAR *lpValueName = new TCHAR[cMaxKeyLen+1];
				for (unsigned int i=0; i<cSubKeys; i++) {
					DWORD len = cMaxKeyLen;
					bRet = RegEnumKey(hTemp, i, lpValueName, len);
					if (bRet != ERROR_SUCCESS) {
						delete [] lpValueName;
						throw settings_exception(_T("Failed to enumerate: ") + path.to_string() + _T(": ") + error::lookup::last_error());
					}
					list.push_back(std::wstring(lpValueName));
				}
				delete [] lpValueName;
			}
		}
		settings::error_list validate() {
			settings::error_list ret;
			return ret;
		}

		virtual std::wstring get_info() {
			return _T("Registry settings: (") + context_ + _T(",TODO)");
		}
public:
		static bool context_exists(settings::settings_core *core, std::wstring key) {
			// @todo: Fix this
			return false;
		}

	};
}

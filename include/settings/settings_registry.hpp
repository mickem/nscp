#pragma once

#include <string>
#include <windows.h>
#include <settings/Settings.h>
#include <msvc_wrappers.h>
#include <error.hpp>
#define BUFF_LEN 4096


namespace Settings {
	class REGSettings : public Settings::SettingsInterfaceImpl {
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
	REGSettings(Settings::SettingsCore *core, std::wstring context) : Settings::SettingsInterfaceImpl(core, context) {}

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
	virtual std::wstring get_real_string(SettingsCore::key_path_type key) {
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
	virtual int get_real_int(SettingsCore::key_path_type key) {
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
	virtual bool get_real_bool(SettingsCore::key_path_type key) {
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
	virtual bool has_real_key(SettingsCore::key_path_type key) {
		return false;
	}
	//////////////////////////////////////////////////////////////////////////
	/// Get the type this settings store represent.
	///
	/// @return the type of settings store
	///
	/// @author mickem
	virtual SettingsCore::settings_type get_type() {
		return SettingsCore::registry;
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
		if (value.type == SettingsCore::key_string) {
			if (!setString_(get_reg_key(key), key.second, value.get_string()))
				throw SettingsException(_T("Failed to write key: ") + key.first + _T(".") + key.second);
		} else if (value.type == SettingsCore::key_integer) {
			if (!setInt_(get_reg_key(key), key.second, value.get_int()))
			throw SettingsException(_T("Failed to write key: ") + key.first + _T(".") + key.second);
		} else if (value.type == SettingsCore::key_bool) {
			if (!setInt_(get_reg_key(key), key.second, value.get_bool()?1:0))
			throw SettingsException(_T("Failed to write key: ") + key.first + _T(".") + key.second);
		} else {
			throw SettingsException(_T("Invalid settings type."));
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
	virtual SettingsCore::key_type get_key_type(std::wstring path, std::wstring key) {
		return SettingsCore::key_string;
	}
private:
	reg_key get_reg_key(SettingsCore::key_path_type key) {
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

	static bool setString_(reg_key path, std::wstring key, std::wstring value) {
		HKEY hTemp;
		if (RegCreateKeyEx(path.hKey, path.path.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hTemp, NULL) != ERROR_SUCCESS) {
			return false;
		}
		DWORD bDataLen = value.length()+2;
		TCHAR *bData = new TCHAR[bDataLen];
		wcsncpy_s(bData, bDataLen, value.c_str(), value.length());
		BOOL bRet = RegSetValueExW(hTemp, key.c_str(), 0, REG_EXPAND_SZ, reinterpret_cast<LPBYTE>(bData), (wcslen(bData)+1)*sizeof(TCHAR));
		RegCloseKey(hTemp);
		delete [] bData;
		return  (bRet == ERROR_SUCCESS);
	}

	static bool setInt_(reg_key path, std::wstring key, DWORD value) {
		HKEY hTemp;
		if (RegCreateKeyEx(path.hKey, path.path.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hTemp, NULL) != ERROR_SUCCESS) {
			return false;
		}
		BOOL bRet = RegSetValueEx(hTemp, key.c_str(), NULL, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(DWORD));
		RegCloseKey(hTemp);
		return  (bRet == ERROR_SUCCESS);
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
				throw SettingsException(_T("String to long: ") + path.to_string());
			} else if (type == REG_DWORD) {
				DWORD dw = *(reinterpret_cast<DWORD*>(bData));
				return strEx::itos(dw);
			}
			throw SettingsException(_T("Unsupported key type: ") + path.to_string());
		} else if (lRet == ERROR_FILE_NOT_FOUND)
			throw KeyNotFoundException(_T("Key not found: ") + path.to_string());
		throw SettingsException(_T("Failed to open key: ") + path.to_string() + _T(": ") + error::lookup::last_error(lRet));
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
					throw SettingsException(_T("Failed to enumerate: ") + path.to_string() + _T(": ") + error::lookup::last_error());
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
					throw SettingsException(_T("Failed to enumerate: ") + path.to_string() + _T(": ") + error::lookup::last_error());
				}
				list.push_back(std::wstring(lpValueName));
			}
			delete [] lpValueName;
		}
	}
	/*
	void setSection(std::wstring section, sectionList data)  {
		std::wcout << _T("Unsupported function call") << std::endl;
	}
	*/
};
}

#pragma once

#include <string>
#include <windows.h>
#include <TSettings.h>
#include <msvc_wrappers.h>
#include <error.hpp>
#define BUFF_LEN 4096


#include <iostream>
class REGSettings : public TSettings
{
public:
	typedef std::list<std::string> sectionList;
	REGSettings(void)
	{
	}

	virtual ~REGSettings(void)
	{
	}

	static bool hasSettings() {
		return getInt_(NS_HKEY_ROOT, NS_REG_ROOT "\\" MAIN_SECTION_TITLE, MAIN_USEREG, 0) == 1;
	}

	std::string getActiveType() {
		return "registry";
	}

	sectionList getSections(unsigned int bufferLength = BUFF_LEN) {
		return getSubKeys_(NS_HKEY_ROOT, NS_REG_ROOT);
	}

	/**
	* Get all keys from a section as a list<string>
	* @param section The section to return all keys from
	* @return A list with all keys from the section
	*/
	sectionList getSection(std::string section, unsigned int bufferLength = BUFF_LEN) {
		return getValues_(NS_HKEY_ROOT, std::string((std::string)NS_REG_ROOT + "\\" + section).c_str());
	}
	/**
	* Get a string from the settings file
	* @param section Section to read from 
	* @param key Key to retrieve
	* @param defaultValue Default value to return if key is not found
	* @return The value or defaultValue if the key is not found
	*/
	std::string getString(std::string section, std::string key, std::string defaultValue = "") const {
		return getString_(NS_HKEY_ROOT, std::string((std::string)NS_REG_ROOT + "\\" + section).c_str(), key.c_str(), defaultValue);
	}

	void setString(std::string section, std::string key, std::string value) {
		setString_(NS_HKEY_ROOT, std::string((std::string)NS_REG_ROOT + "\\" + section).c_str(), key.c_str(), value.c_str());
	}

	/**
	* Get an integer from the settings file
	* @param section Section to read from 
	* @param key Key to retrieve
	* @param defaultValue Default value to return if key is not found
	* @return The value or defaultValue if the key is not found
	*/
	int getInt(std::string section, std::string key, int defaultValue = 0) {
		return getInt_(NS_HKEY_ROOT, std::string((std::string)NS_REG_ROOT + "\\" + section).c_str(), key.c_str(), defaultValue);
	}
	void setInt(std::string section, std::string key, int value) {
		setInt_(NS_HKEY_ROOT, std::string((std::string)NS_REG_ROOT + "\\" + section).c_str(), key.c_str(), value);
	}

	static bool setString_(HKEY hKey, LPCTSTR lpszPath, LPCTSTR lpszKey, LPCTSTR value) {
		HKEY hTemp;
		if (RegCreateKeyEx(hKey, lpszPath, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hTemp, NULL) != ERROR_SUCCESS) {
			return false;
		}
		DWORD cbData = static_cast<DWORD>(strlen(value));
		BYTE *bData = new BYTE[cbData+1];
		strncpy_s(reinterpret_cast<char*>(bData), cbData+1, value, cbData);
		BOOL bRet = RegSetValueEx(hTemp, lpszKey, NULL, REG_SZ, bData, cbData);
		RegCloseKey(hTemp);
		delete [] bData;
		return  (bRet == ERROR_SUCCESS);
	}

	int getActiveTypeID() {
		return REGSettings::getType();
	}
	static int getType() {
		return 2;
	}

	static bool setInt_(HKEY hKey, LPCTSTR lpszPath, LPCTSTR lpszKey, DWORD value) {
		HKEY hTemp;
		if (RegCreateKeyEx(hKey, lpszPath, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hTemp, NULL) != ERROR_SUCCESS) {
			return false;
		}
		BOOL bRet = RegSetValueEx(hTemp, lpszKey, NULL, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(DWORD));
		RegCloseKey(hTemp);
		return  (bRet == ERROR_SUCCESS);
	}

	static std::string getString_(HKEY hKey, LPCTSTR lpszPath, LPCTSTR lpszKey, std::string def) {
		std::string ret = def;
		HKEY hTemp;
		if (RegOpenKeyEx(hKey, lpszPath, 0, KEY_QUERY_VALUE, &hTemp) != ERROR_SUCCESS) {
			return def;
		}
		DWORD type;
		const DWORD data_length = 2048;
		DWORD cbData = data_length;
		BYTE *bData = new BYTE[cbData];
		LONG lRet = RegQueryValueEx(hTemp, lpszKey, NULL, &type, bData, &cbData);
		if (lRet == ERROR_SUCCESS) {
			if (type == REG_SZ) {
				if (cbData < data_length-1) {
					bData[cbData] = 0;
					ret = reinterpret_cast<LPCTSTR>(bData);
				} else {
					std::cout << "getString_::Buffersize to small: " << lpszPath << "." << lpszKey << ": " << type << std::endl;
				}
			} else if (type == REG_DWORD) {
				DWORD dw = *(reinterpret_cast<DWORD*>(bData));
				ret = strEx::itos(dw);
			} else {
				std::cout << "getString_::Unsupported type: " << lpszPath << "." << lpszKey << ": " << type << std::endl;
			}
		} else if (lRet == ERROR_FILE_NOT_FOUND) {
			return def;
		} else {
			std::cout << "getString_::Error: " << lpszPath << "." << lpszKey << ": " << error::format::from_system(lRet) << std::endl;
		}
		RegCloseKey(hTemp);
		delete [] bData;
		return ret;
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
		BYTE *bData = new BYTE[cbData+1];
		bRet = RegQueryValueEx(hTemp, lpszKey, NULL, &type, bData, &cbData);
		if (type != REG_DWORD) {
			bRet = -1;
		}
		RegCloseKey(hTemp);
		if (bRet == ERROR_SUCCESS) {
			ret = static_cast<DWORD>(*bData);
		}
		delete [] bData;
		return ret;
	}
	static sectionList getValues_(HKEY hKey, LPCTSTR lpszPath) {
		sectionList ret;
		LONG bRet;
		HKEY hTemp;
		if ((bRet = RegOpenKeyEx(hKey, lpszPath, 0, KEY_READ, &hTemp)) != ERROR_SUCCESS) {
			return ret;
		}
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
				if (bRet == ERROR_SUCCESS) {
					ret.push_back(std::string(lpValueName));
				} else {
					std::cout << "getValues_::Error: " << bRet << ": " << lpszPath << "[" << i << "]" << std::endl;

				}
			}
			delete [] lpValueName;
		}
		return ret;
	}
	static sectionList getSubKeys_(HKEY hKey, LPCTSTR lpszPath) {
		sectionList ret;
		LONG bRet;
		HKEY hTemp;
		if ((bRet = RegOpenKeyEx(hKey, lpszPath, 0, KEY_READ, &hTemp)) != ERROR_SUCCESS) {
			return ret;
		}
		DWORD    cSubKeys=0;
		DWORD    cMaxKeyLen;
		// Get the class name and the value count. 
		bRet = RegQueryInfoKey(hTemp,NULL,NULL,NULL,&cSubKeys,&cMaxKeyLen,NULL,NULL,NULL,NULL,NULL,NULL);
		cMaxKeyLen++;
		if ((bRet == ERROR_SUCCESS)&&(cSubKeys>0)) {
			TCHAR *lpValueName = new TCHAR[cMaxKeyLen+1];
			for (unsigned int i=0; i<cSubKeys; i++) {
				DWORD len = cMaxKeyLen;
				bRet = RegEnumKey(hTemp, i, lpValueName, len);
				if (bRet == ERROR_SUCCESS) {
					ret.push_back(std::string(lpValueName));
				} else {
					std::cout << "getSubKeys_::Error: " << bRet << ": " << lpszPath << "[" << i << "]" << std::endl;
				}
			}
			delete [] lpValueName;
		}
		return ret;
	}
};

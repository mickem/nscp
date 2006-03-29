#pragma once

#include <string>
#include <windows.h>
#include <TSettings.h>
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
		return getInt_(NS_HKEY_ROOT, NS_REG_ROOT, "use_reg", 0) == 1;
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
	}


	static std::string getString_(HKEY hKey, LPCTSTR lpszPath, LPCTSTR lpszKey, std::string def) {
		std::string ret = def;
		HKEY hTemp;
		if (RegOpenKeyEx(hKey, lpszPath, 0, KEY_QUERY_VALUE, &hTemp) != ERROR_SUCCESS) {
			return def;
		}
		DWORD type;
		DWORD cbData = 1024;
		BYTE *bData = new BYTE[cbData];
		BOOL bRet = RegQueryValueEx(hTemp, lpszKey, NULL, &type, bData, &cbData);
		if (type != REG_SZ) {
			bRet = false;
		}
		RegCloseKey(hTemp);
		if (bRet) {
			ret = (LPCTSTR)bData;
		}
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
		DWORD cbData = 1024;
		BYTE *bData = new BYTE[sizeof(DWORD)];
		bRet = RegQueryValueEx(hTemp, lpszKey, NULL, &type, bData, &cbData);
		if (type != REG_DWORD) {
			bRet = -1;
		}
		RegCloseKey(hTemp);
		if (bRet == ERROR_SUCCESS) {
			ret = (DWORD)*bData;
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
		DWORD    cValues=0;
		DWORD    cMaxValLen;
		// Get the class name and the value count. 
		bRet = RegQueryInfoKey(hTemp,NULL,NULL,NULL,NULL,NULL,NULL,&cValues,&cMaxValLen,NULL,NULL,NULL);
		if ((bRet == ERROR_SUCCESS)&&(cValues>0)) {
			TCHAR *lpValueName = new TCHAR[cMaxValLen+1];
			for (unsigned int i=0; i<cValues; i++) {
				DWORD len = cMaxValLen;
				bRet = RegEnumValue(hKey, i, lpValueName, &len, NULL, NULL, NULL, NULL);
				if (bRet == ERROR_SUCCESS) {
					ret.push_back(std::string(lpValueName));
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
		if ((bRet == ERROR_SUCCESS)&&(cSubKeys>0)) {
			TCHAR *lpValueName = new TCHAR[cMaxKeyLen+1];
			for (unsigned int i=0; i<cSubKeys; i++) {
				DWORD len = cMaxKeyLen;
				bRet = RegEnumKey(hKey, i, lpValueName, len);
				if (bRet == ERROR_SUCCESS) {
					ret.push_back(std::string(lpValueName));
				}
			}
			delete [] lpValueName;
		}
		return ret;
	}
};

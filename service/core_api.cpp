///////////////////////////////////////////////////////////////////////////
// NSClient++ Base Service
// 
// Copyright (c) 2004 MySolutions NORDIC (http://www.medin.name)
//
// Date: 2004-03-13
// Author: Michael Medin (michael@medin.name)
//
// Part of this file is based on work by Bruno Vais (bvais@usa.net)
//
// This software is provided "AS IS", without a warranty of any kind.
// You are free to use/modify this code but leave this header intact.
//
//////////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "NSClient++.h"
#include "core_api.h"
#include <charEx.h>
#include <config.h>
#include <msvc_wrappers.h>
#include <settings/settings_ini.hpp>
#include <settings/settings_registry.hpp>
#include <settings/settings_old.hpp>
#include <Userenv.h>
#include <settings/Settings.h>
#include "settings_manager_impl.h"
#include <b64/b64.h>
#include <NSCHelper.h>



NSCAPI::errorReturn NSAPIGetSettingsString(const TCHAR* section, const TCHAR* key, const TCHAR* defaultValue, TCHAR* buffer, unsigned int bufLen) {
	try {
		return NSCHelper::wrapReturnString(buffer, bufLen, settings_manager::get_settings()->get_string(section, key, defaultValue), NSCAPI::isSuccess);
	} catch (...) {
		LOG_ERROR_STD(_T("Failed to getString: ") + key);
		return NSCAPI::hasFailed;
	}
}
int NSAPIGetSettingsInt(const TCHAR* section, const TCHAR* key, int defaultValue) {
	try {
		return settings_manager::get_settings()->get_int(section, key, defaultValue);
	} catch (SettingsException e) {
		LOG_ERROR_STD(_T("Failed to set settings file") + e.getMessage());
		return defaultValue;
	}
}
NSCAPI::errorReturn NSAPIGetBasePath(TCHAR*buffer, unsigned int bufLen) {
	return NSCHelper::wrapReturnString(buffer, bufLen, mainClient.getBasePath(), NSCAPI::isSuccess);
}
NSCAPI::errorReturn NSAPIGetApplicationName(TCHAR*buffer, unsigned int bufLen) {
	return NSCHelper::wrapReturnString(buffer, bufLen, SZAPPNAME, NSCAPI::isSuccess);
}
NSCAPI::errorReturn NSAPIGetApplicationVersionStr(TCHAR*buffer, unsigned int bufLen) {
	return NSCHelper::wrapReturnString(buffer, bufLen, SZVERSION, NSCAPI::isSuccess);
}
void NSAPIMessage(int msgType, const TCHAR* file, const int line, const TCHAR* message) {
	mainClient.reportMessage(msgType, file, line, message);
}
void NSAPIStopServer(void) {
#ifdef WIN32
	serviceControll::StopNoWait(SZSERVICENAME);
#endif
}
NSCAPI::nagiosReturn NSAPIInject(const TCHAR* command, const unsigned int argLen, TCHAR **argument, TCHAR *returnMessageBuffer, unsigned int returnMessageBufferLen, TCHAR *returnPerfBuffer, unsigned int returnPerfBufferLen) {
	return mainClient.injectRAW(command, argLen, argument, returnMessageBuffer, returnMessageBufferLen, returnPerfBuffer, returnPerfBufferLen);
}
NSCAPI::errorReturn NSAPIGetSettingsSection(const TCHAR* section, TCHAR*** aBuffer, unsigned int * bufLen) {
	try {
		unsigned int len = 0;
		*aBuffer = arrayBuffer::list2arrayBuffer(settings_manager::get_settings()->get_keys(section), len);
		*bufLen = len;
		return NSCAPI::isSuccess;
	} catch (SettingsException e) {
		LOG_ERROR_STD(_T("Failed to get section: ") + e.getMessage());
	} catch (...) {
		LOG_ERROR_STD(_T("Failed to getSection: ") + section);
	}
	return NSCAPI::hasFailed;
}
NSCAPI::errorReturn NSAPIReleaseSettingsSectionBuffer(TCHAR*** aBuffer, unsigned int * bufLen) {
	arrayBuffer::destroyArrayBuffer(*aBuffer, *bufLen);
	*bufLen = 0;
	*aBuffer = NULL;
	return NSCAPI::isSuccess;
}

NSCAPI::boolReturn NSAPICheckLogMessages(int messageType) {
	if (mainClient.logDebug())
		return NSCAPI::istrue;
	return NSCAPI::isfalse;
}

NSCAPI::errorReturn NSAPIEncrypt(unsigned int algorithm, const TCHAR* inBuffer, unsigned int inBufLen, TCHAR* outBuf, unsigned int *outBufLen) {
	if (algorithm != NSCAPI::encryption_xor) {
		LOG_ERROR(_T("Unknown algortihm requested."));
		return NSCAPI::hasFailed;
	}
	/*
	TODO reimplement this

	std::wstring key = settings_manager::get_settings()->get_string(SETTINGS_KEY(protocol_def::MASTER_KEY));
	int tcharInBufLen = 0;
	char *c = charEx::tchar_to_char(inBuffer, inBufLen, tcharInBufLen);
	std::wstring::size_type j=0;
	for (int i=0;i<tcharInBufLen;i++,j++) {
		if (j > key.size())
			j = 0;
		c[i] ^= key[j];
	}
	size_t cOutBufLen = b64::b64_encode(reinterpret_cast<void*>(c), tcharInBufLen, NULL, NULL);
	if (!outBuf) {
		*outBufLen = static_cast<unsigned int>(cOutBufLen*2); // TODO: Guessing wildly here but no proper way to tell without a lot of extra work
		return NSCAPI::isSuccess;
	}
	char *cOutBuf = new char[cOutBufLen+1];
	size_t len = b64::b64_encode(reinterpret_cast<void*>(c), tcharInBufLen, cOutBuf, cOutBufLen);
	delete [] c;
	if (len == 0) {
		LOG_ERROR(_T("Invalid out buffer length."));
		return NSCAPI::isInvalidBufferLen;
	}
	int realOutLen;
	TCHAR *realOut = charEx::char_to_tchar(cOutBuf, cOutBufLen, realOutLen);
	if (static_cast<unsigned int>(realOutLen) >= *outBufLen) {
		LOG_ERROR_STD(_T("Invalid out buffer length: ") + strEx::itos(realOutLen) + _T(" was needed but only ") + strEx::itos(*outBufLen) + _T(" was allocated."));
		return NSCAPI::isInvalidBufferLen;
	}
	wcsncpy_s(outBuf, *outBufLen, realOut, realOutLen);
	delete [] realOut;
	outBuf[realOutLen] = 0;
	*outBufLen = static_cast<unsigned int>(realOutLen);
	*/
	return NSCAPI::isSuccess;
}

NSCAPI::errorReturn NSAPIDecrypt(unsigned int algorithm, const TCHAR* inBuffer, unsigned int inBufLen, TCHAR* outBuf, unsigned int *outBufLen) {
	if (algorithm != NSCAPI::encryption_xor) {
		LOG_ERROR(_T("Unknown algortihm requested."));
		return NSCAPI::hasFailed;
	}
	/*
	int inBufLenC = 0;
	char *inBufferC = charEx::tchar_to_char(inBuffer, inBufLen, inBufLenC);
	size_t cOutLen =  b64::b64_decode(inBufferC, inBufLenC, NULL, NULL);
	if (!outBuf) {
		*outBufLen = static_cast<unsigned int>(cOutLen*2); // TODO: Guessing wildly here but no proper way to tell without a lot of extra work
		return NSCAPI::isSuccess;
	}
	char *cOutBuf = new char[cOutLen+1];
	size_t len = b64::b64_decode(inBufferC, inBufLenC, reinterpret_cast<void*>(cOutBuf), cOutLen);
	delete [] inBufferC;
	if (len == 0) {
		LOG_ERROR(_T("Invalid out buffer length."));
		return NSCAPI::isInvalidBufferLen;
	}
	int realOutLen;

	std::wstring key = settings_manager::get_settings()->get_string(SETTINGS_KEY(protocol_def::MASTER_KEY));
	std::wstring::size_type j=0;
	for (int i=0;i<cOutLen;i++,j++) {
		if (j > key.size())
			j = 0;
		cOutBuf[i] ^= key[j];
	}

	TCHAR *realOut = charEx::char_to_tchar(cOutBuf, cOutLen, realOutLen);
	if (static_cast<unsigned int>(realOutLen) >= *outBufLen) {
		LOG_ERROR_STD(_T("Invalid out buffer length: ") + strEx::itos(realOutLen) + _T(" was needed but only ") + strEx::itos(*outBufLen) + _T(" was allocated."));
		return NSCAPI::isInvalidBufferLen;
	}
	wcsncpy_s(outBuf, *outBufLen, realOut, realOutLen);
	delete [] realOut;
	outBuf[realOutLen] = 0;
	*outBufLen = static_cast<unsigned int>(realOutLen);
	*/
	return NSCAPI::isSuccess;
}

NSCAPI::errorReturn NSAPISetSettingsString(const TCHAR* section, const TCHAR* key, const TCHAR* value) {
	try {
		settings_manager::get_settings()->set_string(section, key, value);
	} catch (...) {
		LOG_ERROR_STD(_T("Failed to setString: ") + key);
		return NSCAPI::hasFailed;
	}
	return NSCAPI::isSuccess;
}
NSCAPI::errorReturn NSAPISetSettingsInt(const TCHAR* section, const TCHAR* key, int value) {
	try {
		settings_manager::get_settings()->set_int(section, key, value);
	} catch (...) {
		LOG_ERROR_STD(_T("Failed to setInt: ") + key);
		return NSCAPI::hasFailed;
	}
	return NSCAPI::isSuccess;
}
NSCAPI::errorReturn NSAPIWriteSettings(int type) {
	try {
		if (type == NSCAPI::settings_registry)
			settings_manager::get_core()->migrate_to(Settings::SettingsCore::registry);
		else if (type == NSCAPI::settings_inifile)
			settings_manager::get_core()->migrate_to(Settings::SettingsCore::ini_file);
		else
			settings_manager::get_settings()->save();
	} catch (SettingsException e) {
		LOG_ERROR_STD(_T("Failed to write settings: ") + e.getMessage());
		return NSCAPI::hasFailed;
	} catch (...) {
		LOG_ERROR_STD(_T("Failed to write settings"));
		return NSCAPI::hasFailed;
	}
	return NSCAPI::isSuccess;
}
NSCAPI::errorReturn NSAPIReadSettings(int type) {
	try {
		if (type == NSCAPI::settings_registry)
			settings_manager::get_core()->migrate_from(Settings::SettingsCore::registry);
		else if (type == NSCAPI::settings_inifile)
			settings_manager::get_core()->migrate_from(Settings::SettingsCore::ini_file);
		else
			settings_manager::get_settings()->reload();
	} catch (SettingsException e) {
		LOG_ERROR_STD(_T("Failed to read settings: ") + e.getMessage());
		return NSCAPI::hasFailed;
	} catch (...) {
		LOG_ERROR_STD(_T("Failed to read settings"));
		return NSCAPI::hasFailed;
	}
	return NSCAPI::isSuccess;
}
NSCAPI::errorReturn NSAPIRehash(int flag) {
	return NSCAPI::hasFailed;
}
NSCAPI::errorReturn NSAPIDescribeCommand(const TCHAR* command, TCHAR* buffer, unsigned int bufLen) {
	return NSCHelper::wrapReturnString(buffer, bufLen, mainClient.describeCommand(command), NSCAPI::isSuccess);
}
NSCAPI::errorReturn NSAPIGetAllCommandNames(arrayBuffer::arrayBuffer* aBuffer, unsigned int *bufLen) {
	unsigned int len = 0;
	*aBuffer = arrayBuffer::list2arrayBuffer(mainClient.getAllCommandNames(), len);
	*bufLen = len;
	return NSCAPI::isSuccess;
}
NSCAPI::errorReturn NSAPIReleaseAllCommandNamessBuffer(TCHAR*** aBuffer, unsigned int * bufLen) {
	arrayBuffer::destroyArrayBuffer(*aBuffer, *bufLen);
	*bufLen = 0;
	*aBuffer = NULL;
	return NSCAPI::isSuccess;
}
NSCAPI::errorReturn NSAPIRegisterCommand(const TCHAR* cmd,const TCHAR* desc) {
	mainClient.registerCommand(cmd, desc);
	return NSCAPI::isSuccess;
}
NSCAPI::errorReturn NSAPISettingsRegKey(const TCHAR* path, const TCHAR* key, int type, const TCHAR* title, const TCHAR* description, const TCHAR* defVal, int advanced) {
	try {
		if (type == NSCAPI::key_string)
			settings_manager::get_core()->register_key(path, key, Settings::SettingsCore::key_string, title, description, defVal, advanced);
		if (type == NSCAPI::key_bool)
			settings_manager::get_core()->register_key(path, key, Settings::SettingsCore::key_bool, title, description, defVal, advanced);
		if (type == NSCAPI::key_integer)
			settings_manager::get_core()->register_key(path, key, Settings::SettingsCore::key_integer, title, description, defVal, advanced);
		return NSCAPI::hasFailed;
	} catch (SettingsException e) {
		LOG_ERROR_STD(_T("Failed register key: ") + e.getMessage());
		return NSCAPI::hasFailed;
	} catch (...) {
		LOG_ERROR_STD(_T("Failed register key"));
		return NSCAPI::hasFailed;
	}
	return NSCAPI::isSuccess;
}


NSCAPI::errorReturn NSAPISettingsRegPath(const TCHAR* path, const TCHAR* title, const TCHAR* description, int advanced) {
	try {
		settings_manager::get_core()->register_path(path, title, description, advanced);
	} catch (SettingsException e) {
		LOG_ERROR_STD(_T("Failed register path: ") + e.getMessage());
		return NSCAPI::hasFailed;
	} catch (...) {
		LOG_ERROR_STD(_T("Failed register path"));
		return NSCAPI::hasFailed;
	}
	return NSCAPI::isSuccess;
}

//int wmain(int argc, TCHAR* argv[], TCHAR* envp[])
TCHAR* copyString(const std::wstring &str) {
	int sz = str.size();
	TCHAR *tc = new TCHAR[sz+2];
	wcsncpy_s(tc, sz+1, str.c_str(), sz);
	return tc;
}
NSCAPI::errorReturn NSAPIGetPluginList(int *len, NSCAPI::plugin_info *list[]) {
	NSClientT::plugin_info_list plugList= mainClient.get_all_plugins();
	*len = plugList.size();

	*list = new NSCAPI::plugin_info[*len+1];
	int i=0;
	for(NSClientT::plugin_info_list::const_iterator cit = plugList.begin(); cit != plugList.end(); ++cit,i++) {
		(*list)[i].dll = copyString((*cit).dll);
		(*list)[i].name = copyString((*cit).name);
		(*list)[i].version = copyString((*cit).version);
		(*list)[i].description = copyString((*cit).description);
	}
	return NSCAPI::isSuccess;
}
NSCAPI::errorReturn NSAPIReleasePluginList(int len, NSCAPI::plugin_info *list[]) {
	for (int i=0;i<len;i++) {
		delete [] (*list)[i].dll;
		delete [] (*list)[i].name;
		delete [] (*list)[i].version;
		delete [] (*list)[i].description;
	}
	delete [] *list;
	return NSCAPI::isSuccess;
}


NSCAPI::errorReturn NSAPISettingsSave(void) {
	try {
		settings_manager::get_settings()->save();
	} catch (SettingsException e) {
		LOG_ERROR_STD(_T("Failed to save: ") + e.getMessage());
		return NSCAPI::hasFailed;
	} catch (...) {
		LOG_ERROR_STD(_T("Failed to save"));
		return NSCAPI::hasFailed;
	}
	return NSCAPI::isSuccess;
}



LPVOID NSAPILoader(TCHAR*buffer) {
	if (_wcsicmp(buffer, _T("NSAPIGetApplicationName")) == 0)
		return &NSAPIGetApplicationName;
	if (_wcsicmp(buffer, _T("NSAPIGetApplicationVersionStr")) == 0)
		return &NSAPIGetApplicationVersionStr;
	if (_wcsicmp(buffer, _T("NSAPIGetSettingsString")) == 0)
		return &NSAPIGetSettingsString;
	if (_wcsicmp(buffer, _T("NSAPIGetSettingsSection")) == 0)
		return &NSAPIGetSettingsSection;
	if (_wcsicmp(buffer, _T("NSAPIReleaseSettingsSectionBuffer")) == 0)
		return &NSAPIReleaseSettingsSectionBuffer;
	if (_wcsicmp(buffer, _T("NSAPIGetSettingsInt")) == 0)
		return &NSAPIGetSettingsInt;
	if (_wcsicmp(buffer, _T("NSAPIMessage")) == 0)
		return &NSAPIMessage;
	if (_wcsicmp(buffer, _T("NSAPIStopServer")) == 0)
		return &NSAPIStopServer;
	if (_wcsicmp(buffer, _T("NSAPIInject")) == 0)
		return &NSAPIInject;
	if (_wcsicmp(buffer, _T("NSAPIGetBasePath")) == 0)
		return &NSAPIGetBasePath;
	if (_wcsicmp(buffer, _T("NSAPICheckLogMessages")) == 0)
		return &NSAPICheckLogMessages;
	if (_wcsicmp(buffer, _T("NSAPIEncrypt")) == 0)
		return &NSAPIEncrypt;
	if (_wcsicmp(buffer, _T("NSAPIDecrypt")) == 0)
		return &NSAPIDecrypt;
	if (_wcsicmp(buffer, _T("NSAPISetSettingsString")) == 0)
		return &NSAPISetSettingsString;
	if (_wcsicmp(buffer, _T("NSAPISetSettingsInt")) == 0)
		return &NSAPISetSettingsInt;
	if (_wcsicmp(buffer, _T("NSAPIWriteSettings")) == 0)
		return &NSAPIWriteSettings;
	if (_wcsicmp(buffer, _T("NSAPIReadSettings")) == 0)
		return &NSAPIReadSettings;
	if (_wcsicmp(buffer, _T("NSAPIRehash")) == 0)
		return &NSAPIRehash;
	if (_wcsicmp(buffer, _T("NSAPIDescribeCommand")) == 0)
		return &NSAPIDescribeCommand;
	if (_wcsicmp(buffer, _T("NSAPIGetAllCommandNames")) == 0)
		return &NSAPIGetAllCommandNames;
	if (_wcsicmp(buffer, _T("NSAPIReleaseAllCommandNamessBuffer")) == 0)
		return &NSAPIReleaseAllCommandNamessBuffer;
	if (_wcsicmp(buffer, _T("NSAPIRegisterCommand")) == 0)
		return &NSAPIRegisterCommand;
	if (_wcsicmp(buffer, _T("NSAPISettingsRegKey")) == 0)
		return &NSAPISettingsRegKey;
	if (_wcsicmp(buffer, _T("NSAPISettingsRegPath")) == 0)
		return &NSAPISettingsRegPath;
	if (_wcsicmp(buffer, _T("NSAPIGetPluginList")) == 0)
		return &NSAPIGetPluginList;
	if (_wcsicmp(buffer, _T("NSAPIReleasePluginList")) == 0)
		return &NSAPIReleasePluginList;
	if (_wcsicmp(buffer, _T("NSAPISettingsSave")) == 0)
		return &NSAPISettingsSave;

	LOG_ERROR_STD(_T("Function not found: ") + buffer);
	return NULL;
}



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
#include "StdAfx.h"
#include "NSClient++.h"
#include "core_api.h"
#include <charEx.h>
#include <config.h>
#include "../version.hpp"
#include <arrayBuffer.h>
#include <settings/settings_core.hpp>
#include "../helpers/settings_manager/settings_manager_impl.h"
#include <b64/b64.h>
#include <nscapi/nscapi_helper.hpp>
#ifdef _WIN32
#include <ServiceCmd.h>
#endif
#include "logger.hpp"

using namespace nscp::helpers;

#define LOG_ERROR_STD(msg) LOG_ERROR(((std::wstring)msg).c_str())
#define LOG_CRITICAL_STD(msg) LOG_CRITICAL(((std::wstring)msg).c_str())
#define LOG_MESSAGE_STD(msg) LOG_MESSAGE(((std::wstring)msg).c_str())
#define LOG_DEBUG_STD(msg) LOG_DEBUG(((std::wstring)msg).c_str())

#define LOG_ERROR(msg) { std::string s = nsclient::logger_helper::create_error(__FILE__, __LINE__, msg); mainClient.reportMessage(s); }
#define LOG_CRITICAL(msg) { std::string s = nsclient::logger_helper::create_error(__FILE__, __LINE__, msg); mainClient.reportMessage(s); }
#define LOG_MESSAGE(msg) { std::string s = nsclient::logger_helper::create_info(__FILE__, __LINE__, msg); mainClient.reportMessage(s); }
#define LOG_DEBUG(msg) { std::string s = nsclient::logger_helper::create_debug(__FILE__, __LINE__, msg); mainClient.reportMessage(s); }

NSCAPI::errorReturn NSAPIExpandPath(const wchar_t* key, wchar_t* buffer,unsigned int bufLen) {
	try {
		return nscapi::plugin_helper::wrapReturnString(buffer, bufLen, mainClient.expand_path(key), NSCAPI::isSuccess);
	} catch (...) {
		LOG_ERROR_STD(_T("Failed to getString: ") + key);
		return NSCAPI::hasFailed;
	}
}

NSCAPI::errorReturn NSAPIGetSettingsString(const wchar_t* section, const wchar_t* key, const wchar_t* defaultValue, wchar_t* buffer, unsigned int bufLen) {
	try {
		return nscapi::plugin_helper::wrapReturnString(buffer, bufLen, settings_manager::get_settings()->get_string(section, key, defaultValue), NSCAPI::isSuccess);
	} catch (settings::settings_exception e) {
		LOG_ERROR_STD(_T("Failed to set settings file") + e.getMessage());
		return NSCAPI::hasFailed;
	} catch (const std::exception &e) {
		LOG_ERROR_STD(_T("Failed to get key: ") + utf8::cvt<std::wstring>(e.what()));
		return NSCAPI::hasFailed;
	} catch (...) {
		LOG_ERROR_STD(_T("Failed to get key: <UNKNOWN EXCEPTION>"));
		return NSCAPI::hasFailed;
	}
}
int NSAPIGetSettingsInt(const wchar_t* section, const wchar_t* key, int defaultValue) {
	try {
		return settings_manager::get_settings()->get_int(section, key, defaultValue);
	} catch (settings::settings_exception e) {
		LOG_ERROR_STD(_T("Failed to set settings file") + e.getMessage());
		return defaultValue;
	} catch (const std::exception &e) {
		LOG_ERROR_STD(_T("Failed to get key: ") + utf8::cvt<std::wstring>(e.what()));
		return defaultValue;
	} catch (...) {
		LOG_ERROR_STD(_T("Failed to get key: <UNKNOWN EXCEPTION>"));
		return defaultValue;
	}
}
int NSAPIGetSettingsBool(const wchar_t* section, const wchar_t* key, int defaultValue) {
	try {
		return settings_manager::get_settings()->get_bool(section, key, defaultValue==1);
	} catch (settings::settings_exception e) {
		LOG_ERROR_STD(_T("Failed to get key: ") + e.getMessage());
		return defaultValue;
	} catch (const std::exception &e) {
		LOG_ERROR_STD(_T("Failed to get key: ") + utf8::cvt<std::wstring>(e.what()));
		return defaultValue;
	} catch (...) {
		LOG_ERROR_STD(_T("Failed to get key: <UNKNOWN EXCEPTION>"));
		return defaultValue;
	}
}
NSCAPI::errorReturn NSAPIGetBasePath(wchar_t*buffer, unsigned int bufLen) {
	return nscapi::plugin_helper::wrapReturnString(buffer, bufLen, mainClient.getBasePath().string(), NSCAPI::isSuccess);
}
NSCAPI::errorReturn NSAPIGetApplicationName(wchar_t*buffer, unsigned int bufLen) {
	return nscapi::plugin_helper::wrapReturnString(buffer, bufLen, APPLICATION_NAME, NSCAPI::isSuccess);
}
NSCAPI::errorReturn NSAPIGetApplicationVersionStr(wchar_t*buffer, unsigned int bufLen) {
	return nscapi::plugin_helper::wrapReturnString(buffer, bufLen, CURRENT_SERVICE_VERSION, NSCAPI::isSuccess);
}
void NSAPIMessage(const char* data, unsigned int count) {
	mainClient.reportMessage(std::string(data, count));
}
void NSAPIStopServer(void) {
	mainClient.get_service_control().stop();
}
NSCAPI::nagiosReturn NSAPIInject(const wchar_t* command, const char *request_buffer, const unsigned int request_buffer_len, char **response_buffer, unsigned int *response_buffer_len) {
	std::string request (request_buffer, request_buffer_len), response;
	NSCAPI::nagiosReturn ret = mainClient.injectRAW(command, request, response);
	*response_buffer_len = response.size();
	if (response.empty())
		*response_buffer = NULL;
	else {
		*response_buffer = new char[*response_buffer_len + 10];
		memcpy(*response_buffer, response.c_str(), *response_buffer_len);
	}
	return ret;
}
NSCAPI::errorReturn NSAPIGetSettingsSection(const wchar_t* section, wchar_t*** aBuffer, unsigned int * bufLen) {
	try {
		unsigned int len = 0;
		*aBuffer = array_buffer::list2arrayBuffer(settings_manager::get_settings()->get_keys(section), len);
		*bufLen = len;
		return NSCAPI::isSuccess;
	} catch (settings::settings_exception e) {
		LOG_ERROR_STD(_T("Failed to get section: ") + e.getMessage());
	} catch (...) {
		LOG_ERROR_STD(_T("Failed to getSection: ") + section);
	}
	return NSCAPI::hasFailed;
}
NSCAPI::errorReturn NSAPIReleaseSettingsSectionBuffer(wchar_t*** aBuffer, unsigned int * bufLen) {
	array_buffer::destroyArrayBuffer(*aBuffer, *bufLen);
	*bufLen = 0;
	*aBuffer = NULL;
	return NSCAPI::isSuccess;
}

NSCAPI::boolReturn NSAPICheckLogMessages(int messageType) {
	if (mainClient.logDebug())
		return NSCAPI::istrue;
	return NSCAPI::isfalse;
}

NSCAPI::errorReturn NSAPIEncrypt(unsigned int algorithm, const wchar_t* inBuffer, unsigned int inBufLen, wchar_t* outBuf, unsigned int *outBufLen) {
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
	wchar_t *realOut = charEx::char_to_tchar(cOutBuf, cOutBufLen, realOutLen);
	if (static_cast<unsigned int>(realOutLen) >= *outBufLen) {
		LOG_ERROR_STD(_T("Invalid out buffer length: ") + strEx::itos(realOutLen) + _T(" was needed but only ") + strEx::itos(*outBufLen) + _T(" was allocated."));
		return NSCAPI::isInvalidBufferLen;
	}
	wcsncpy(outBuf, *outBufLen, realOut);
	delete [] realOut;
	outBuf[realOutLen] = 0;
	*outBufLen = static_cast<unsigned int>(realOutLen);
	*/
	return NSCAPI::isSuccess;
}

NSCAPI::errorReturn NSAPIDecrypt(unsigned int algorithm, const wchar_t* inBuffer, unsigned int inBufLen, wchar_t* outBuf, unsigned int *outBufLen) {
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

	wchar_t *realOut = charEx::char_to_tchar(cOutBuf, cOutLen, realOutLen);
	if (static_cast<unsigned int>(realOutLen) >= *outBufLen) {
		LOG_ERROR_STD(_T("Invalid out buffer length: ") + strEx::itos(realOutLen) + _T(" was needed but only ") + strEx::itos(*outBufLen) + _T(" was allocated."));
		return NSCAPI::isInvalidBufferLen;
	}
	wcsncpy(outBuf, *outBufLen, realOut);
	delete [] realOut;
	outBuf[realOutLen] = 0;
	*outBufLen = static_cast<unsigned int>(realOutLen);
	*/
	return NSCAPI::isSuccess;
}

NSCAPI::errorReturn NSAPISetSettingsString(const wchar_t* section, const wchar_t* key, const wchar_t* value) {
	try {
		settings_manager::get_settings()->set_string(section, key, value);
	} catch (...) {
		LOG_ERROR_STD(_T("Failed to setString: ") + key);
		return NSCAPI::hasFailed;
	}
	return NSCAPI::isSuccess;
}
NSCAPI::errorReturn NSAPISetSettingsInt(const wchar_t* section, const wchar_t* key, int value) {
	try {
		settings_manager::get_settings()->set_int(section, key, value);
	} catch (...) {
		LOG_ERROR_STD(_T("Failed to setInt: ") + key);
		return NSCAPI::hasFailed;
	}
	return NSCAPI::isSuccess;
}
NSCAPI::errorReturn NSAPIWriteSettings(const wchar_t* key) {
	try {
		settings::instance_ptr inst = settings_manager::get_core()->create_instance(key);
		if (!inst) {
			LOG_ERROR_STD(_T("Failed to create settings: ") + key);
			return NSCAPI::hasFailed;
		}
		settings_manager::get_core()->migrate_to(inst);
	} catch (settings::settings_exception e) {
		LOG_ERROR_STD(_T("Failed to write settings: ") + e.getMessage());
		return NSCAPI::hasFailed;
	} catch (...) {
		LOG_ERROR_STD(_T("Failed to write settings"));
		return NSCAPI::hasFailed;
	}
	return NSCAPI::isSuccess;
}
NSCAPI::errorReturn NSAPIReadSettings(const wchar_t* key) {
	try {
		settings::instance_ptr inst = settings_manager::get_core()->create_instance(key);
		if (!inst) {
			LOG_ERROR_STD(_T("Failed to create settings: ") + key);
			return NSCAPI::hasFailed;
		}
		settings_manager::get_core()->migrate_from(inst);
		settings_manager::get_settings()->reload();
	} catch (settings::settings_exception e) {
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
NSCAPI::errorReturn NSAPIDescribeCommand(const wchar_t* command, wchar_t* buffer, unsigned int bufLen) {
	return nscapi::plugin_helper::wrapReturnString(buffer, bufLen, mainClient.describeCommand(command), NSCAPI::isSuccess);
}
NSCAPI::errorReturn NSAPIGetAllCommandNames(array_buffer::arrayBuffer* aBuffer, unsigned int *bufLen) {
	unsigned int len = 0;
	*aBuffer = array_buffer::list2arrayBuffer(mainClient.getAllCommandNames(), len);
	*bufLen = len;
	return NSCAPI::isSuccess;
}
NSCAPI::errorReturn NSAPIReleaseAllCommandNamessBuffer(wchar_t*** aBuffer, unsigned int * bufLen) {
	array_buffer::destroyArrayBuffer(*aBuffer, *bufLen);
	*bufLen = 0;
	*aBuffer = NULL;
	return NSCAPI::isSuccess;
}
NSCAPI::errorReturn NSAPIRegisterCommand(unsigned int id, const wchar_t* cmd,const wchar_t* desc) {
	try {
		mainClient.registerCommand(id, cmd, desc);
	} catch (nsclient::commands::command_exception &e) {
		LOG_ERROR_STD(_T("Exception registrying command '") + cmd + _T("': ") + ::to_wstring(e.what()) + _T(", from: ") + to_wstring(id));
		return NSCAPI::isfalse;
	} catch (...) {
		LOG_ERROR_STD(_T("Unknown exception registrying command: ") + std::wstring(cmd) + _T(", from: ") + to_wstring(id));
		return NSCAPI::isfalse;
	}
	return NSCAPI::isSuccess;
}
NSCAPI::errorReturn NSAPISettingsRegKey(const wchar_t* path, const wchar_t* key, int type, const wchar_t* title, const wchar_t* description, const wchar_t* defVal, int advanced) {
	try {
		if (type == NSCAPI::key_string)
			settings_manager::get_core()->register_key(path, key, settings::settings_core::key_string, title, description, defVal, advanced);
		if (type == NSCAPI::key_bool)
			settings_manager::get_core()->register_key(path, key, settings::settings_core::key_bool, title, description, defVal, advanced);
		if (type == NSCAPI::key_integer)
			settings_manager::get_core()->register_key(path, key, settings::settings_core::key_integer, title, description, defVal, advanced);
		return NSCAPI::hasFailed;
	} catch (settings::settings_exception e) {
		LOG_ERROR_STD(_T("Failed register key: ") + e.getMessage());
		return NSCAPI::hasFailed;
	} catch (...) {
		LOG_ERROR_STD(_T("Failed register key"));
		return NSCAPI::hasFailed;
	}
	return NSCAPI::isSuccess;
}


NSCAPI::errorReturn NSAPISettingsRegPath(const wchar_t* path, const wchar_t* title, const wchar_t* description, int advanced) {
	try {
		settings_manager::get_core()->register_path(path, title, description, advanced);
	} catch (settings::settings_exception e) {
		LOG_ERROR_STD(_T("Failed register path: ") + e.getMessage());
		return NSCAPI::hasFailed;
	} catch (...) {
		LOG_ERROR_STD(_T("Failed register path"));
		return NSCAPI::hasFailed;
	}
	return NSCAPI::isSuccess;
}

//int wmain(int argc, wchar_t* argv[], wchar_t* envp[])
wchar_t* copyString(const std::wstring &str) {
	int sz = str.size();
	wchar_t *tc = new wchar_t[sz+2];
	wcsncpy(tc, str.c_str(), sz);
	return tc;
}
NSCAPI::errorReturn NSAPIGetPluginList(int *len, NSCAPI::plugin_info *list[]) {
//	NSClientT::plugin_info_list plugList= mainClient.get_all_plugins();
	*len = 0; //plugList.size();

	*list = new NSCAPI::plugin_info[*len+1];
	/*
	int i=0;
	for(NSClientT::plugin_info_list::const_iterator cit = plugList.begin(); cit != plugList.end(); ++cit,i++) {
		(*list)[i].dll = copyString((*cit).dll);
		(*list)[i].name = copyString((*cit).name);
		(*list)[i].version = copyString((*cit).version);
		(*list)[i].description = copyString((*cit).description);
	}
	*/
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
	} catch (settings::settings_exception e) {
		LOG_ERROR_STD(_T("Failed to save: ") + e.getMessage());
		return NSCAPI::hasFailed;
	} catch (...) {
		LOG_ERROR_STD(_T("Failed to save"));
		return NSCAPI::hasFailed;
	}
	return NSCAPI::isSuccess;
}



LPVOID NSAPILoader(const wchar_t*buffer) {
	if (wcscasecmp(buffer, _T("NSAPIGetApplicationName")) == 0)
		return reinterpret_cast<LPVOID>(&NSAPIGetApplicationName);
	if (wcscasecmp(buffer, _T("NSAPIGetApplicationVersionStr")) == 0)
		return reinterpret_cast<LPVOID>(&NSAPIGetApplicationVersionStr);
	if (wcscasecmp(buffer, _T("NSAPIGetSettingsString")) == 0)
		return reinterpret_cast<LPVOID>(&NSAPIGetSettingsString);
	if (wcscasecmp(buffer, _T("NSAPIGetSettingsSection")) == 0)
		return reinterpret_cast<LPVOID>(&NSAPIGetSettingsSection);
	if (wcscasecmp(buffer, _T("NSAPIReleaseSettingsSectionBuffer")) == 0)
		return reinterpret_cast<LPVOID>(&NSAPIReleaseSettingsSectionBuffer);
	if (wcscasecmp(buffer, _T("NSAPIGetSettingsInt")) == 0)
		return reinterpret_cast<LPVOID>(&NSAPIGetSettingsInt);
	if (wcscasecmp(buffer, _T("NSAPIGetSettingsBool")) == 0)
		return reinterpret_cast<LPVOID>(&NSAPIGetSettingsBool);
	if (wcscasecmp(buffer, _T("NSAPIMessage")) == 0)
		return reinterpret_cast<LPVOID>(&NSAPIMessage);
	if (wcscasecmp(buffer, _T("NSAPIStopServer")) == 0)
		return reinterpret_cast<LPVOID>(&NSAPIStopServer);
	if (wcscasecmp(buffer, _T("NSAPIInject")) == 0)
		return reinterpret_cast<LPVOID>(&NSAPIInject);
	if (wcscasecmp(buffer, _T("NSAPIGetBasePath")) == 0)
		return reinterpret_cast<LPVOID>(&NSAPIGetBasePath);
	if (wcscasecmp(buffer, _T("NSAPICheckLogMessages")) == 0)
		return reinterpret_cast<LPVOID>(&NSAPICheckLogMessages);
	if (wcscasecmp(buffer, _T("NSAPIEncrypt")) == 0)
		return reinterpret_cast<LPVOID>(&NSAPIEncrypt);
	if (wcscasecmp(buffer, _T("NSAPIDecrypt")) == 0)
		return reinterpret_cast<LPVOID>(&NSAPIDecrypt);
	if (wcscasecmp(buffer, _T("NSAPISetSettingsString")) == 0)
		return reinterpret_cast<LPVOID>(&NSAPISetSettingsString);
	if (wcscasecmp(buffer, _T("NSAPISetSettingsInt")) == 0)
		return reinterpret_cast<LPVOID>(&NSAPISetSettingsInt);
	if (wcscasecmp(buffer, _T("NSAPIWriteSettings")) == 0)
		return reinterpret_cast<LPVOID>(&NSAPIWriteSettings);
	if (wcscasecmp(buffer, _T("NSAPIReadSettings")) == 0)
		return reinterpret_cast<LPVOID>(&NSAPIReadSettings);
	if (wcscasecmp(buffer, _T("NSAPIRehash")) == 0)
		return reinterpret_cast<LPVOID>(&NSAPIRehash);
	if (wcscasecmp(buffer, _T("NSAPIDescribeCommand")) == 0)
		return reinterpret_cast<LPVOID>(&NSAPIDescribeCommand);
	if (wcscasecmp(buffer, _T("NSAPIGetAllCommandNames")) == 0)
		return reinterpret_cast<LPVOID>(&NSAPIGetAllCommandNames);
	if (wcscasecmp(buffer, _T("NSAPIReleaseAllCommandNamessBuffer")) == 0)
		return reinterpret_cast<LPVOID>(&NSAPIReleaseAllCommandNamessBuffer);
	if (wcscasecmp(buffer, _T("NSAPIRegisterCommand")) == 0)
		return reinterpret_cast<LPVOID>(&NSAPIRegisterCommand);
	if (wcscasecmp(buffer, _T("NSAPISettingsRegKey")) == 0)
		return reinterpret_cast<LPVOID>(&NSAPISettingsRegKey);
	if (wcscasecmp(buffer, _T("NSAPISettingsRegPath")) == 0)
		return reinterpret_cast<LPVOID>(&NSAPISettingsRegPath);
	if (wcscasecmp(buffer, _T("NSAPIGetPluginList")) == 0)
		return reinterpret_cast<LPVOID>(&NSAPIGetPluginList);
	if (wcscasecmp(buffer, _T("NSAPIReleasePluginList")) == 0)
		return reinterpret_cast<LPVOID>(&NSAPIReleasePluginList);
	if (wcscasecmp(buffer, _T("NSAPISettingsSave")) == 0)
		return reinterpret_cast<LPVOID>(&NSAPISettingsSave);
	if (wcscasecmp(buffer, _T("NSAPINotify")) == 0)
		return reinterpret_cast<LPVOID>(&NSAPINotify);
	if (wcscasecmp(buffer, _T("NSAPIDestroyBuffer")) == 0)
		return reinterpret_cast<LPVOID>(&NSAPIDestroyBuffer);
	if (wcscasecmp(buffer, _T("NSAPIExpandPath")) == 0)
		return reinterpret_cast<LPVOID>(&NSAPIExpandPath);

	LOG_ERROR_STD(_T("Function not found: ") + buffer);
	return NULL;
}

NSCAPI::errorReturn NSAPINotify(const wchar_t* channel, const wchar_t* command, NSCAPI::nagiosReturn code,  char* result, unsigned int result_len) {
	return mainClient.send_notification(channel, command, code, result, result_len);
}

void NSAPIDestroyBuffer(char**buffer) {
	delete [] *buffer;
}
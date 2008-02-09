/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#pragma once

#include <string>
#include <list>
#include <NSCAPI.h>
#include <iostream>
#include <charEx.h>
#include <arrayBuffer.h>
#include <windows.h>


namespace NSCHelper
{
#ifdef DEBUG
	NSCAPI::nagiosReturn wrapReturnString(TCHAR *buffer, unsigned int bufLen, std::wstring str, NSCAPI::nagiosReturn defaultReturnCode);
	NSCAPI::errorReturn wrapReturnString(TCHAR *buffer, unsigned int bufLen, std::wstring str, NSCAPI::errorReturn defaultReturnCode);
#else
	int wrapReturnString(TCHAR *buffer, unsigned int bufLen, std::wstring str, int defaultReturnCode);
#endif
	std::wstring translateMessageType(NSCAPI::messageTypes msgType);
	std::wstring translateReturn(NSCAPI::nagiosReturn returnCode);
	NSCAPI::nagiosReturn maxState(NSCAPI::nagiosReturn a, NSCAPI::nagiosReturn b);

	inline bool isNagiosReturnCode(NSCAPI::nagiosReturn code) {
		if ( (code == NSCAPI::returnOK) || (code == NSCAPI::returnWARN) || (code == NSCAPI::returnCRIT) || (code == NSCAPI::returnUNKNOWN) )
			return true;
		return false;
	}
	inline bool isMyNagiosReturn(NSCAPI::nagiosReturn code) {
		return code == NSCAPI::returnCRIT || code == NSCAPI::returnOK || code == NSCAPI::returnWARN || code == NSCAPI::returnUNKNOWN 
			|| code == NSCAPI::returnInvalidBufferLen || code == NSCAPI::returnIgnored;
	}

#ifdef DEBUG
	inline NSCAPI::nagiosReturn int2nagios(int code) {
		if (code == 0)
			return NSCAPI::returnOK;
		if (code == 1)
			return NSCAPI::returnWARN;
		if (code == 2)
			return NSCAPI::returnCRIT;
		if (code == 4)
			return NSCAPI::returnUNKNOWN;
		throw "@fixme bad code";
	}
	inline int nagios2int(NSCAPI::nagiosReturn code) {
		if (code == NSCAPI::returnOK)
			return 0;
		if (code == NSCAPI::returnWARN)
			return 1;
		if (code == NSCAPI::returnCRIT)
			return 2;
		if (code == NSCAPI::returnUNKNOWN)
			return 4;
		throw "@fixme bad code";
	}
#else
	inline NSCAPI::nagiosReturn int2nagios(int code) {
		return code;
	}
	inline int nagios2int(NSCAPI::nagiosReturn code) {
		return code;
	}
#endif
	inline void escalteReturnCodeToCRIT(NSCAPI::nagiosReturn &currentReturnCode) {
		currentReturnCode = NSCAPI::returnCRIT;
	}
	inline void escalteReturnCodeToWARN(NSCAPI::nagiosReturn &currentReturnCode) {
		if (currentReturnCode != NSCAPI::returnCRIT)
			currentReturnCode = NSCAPI::returnWARN;
	}
};

namespace NSCModuleHelper
{
	class NSCMHExcpetion {
	public:
		std::wstring msg_;
		NSCMHExcpetion(std::wstring msg) : msg_(msg) {}
	};
	// Types for the Callbacks into the main program
	typedef NSCAPI::errorReturn (*lpNSAPIGetBasePath)(TCHAR*,unsigned int);
	typedef NSCAPI::errorReturn (*lpNSAPIGetApplicationName)(TCHAR*,unsigned int);
	typedef NSCAPI::errorReturn (*lpNSAPIGetApplicationVersionStr)(TCHAR*,unsigned int);
	typedef NSCAPI::errorReturn (*lpNSAPIGetSettingsString)(const TCHAR*,const TCHAR*,const TCHAR*,TCHAR*,unsigned int);
	typedef NSCAPI::errorReturn (*lpNSAPIGetSettingsInt)(const TCHAR*, const TCHAR*, int);
	typedef NSCAPI::errorReturn (*lpNSAPIGetSettingsSection)(const TCHAR*, arrayBuffer::arrayBuffer*, unsigned int *);
	typedef NSCAPI::errorReturn (*lpNSAPIReleaseSettingsSectionBuffer)(arrayBuffer::arrayBuffer*, unsigned int *);
	typedef void (*lpNSAPIMessage)(int, const TCHAR*, const int, const TCHAR*);
	typedef NSCAPI::errorReturn (*lpNSAPIStopServer)(void);
	typedef NSCAPI::nagiosReturn (*lpNSAPIInject)(const TCHAR*, const unsigned int, TCHAR **, TCHAR *, unsigned int, TCHAR *, unsigned int);
	typedef void* (*lpNSAPILoader)(TCHAR*);
	typedef NSCAPI::boolReturn (*lpNSAPICheckLogMessages)(int);
	typedef NSCAPI::errorReturn (*lpNSAPIEncrypt)(unsigned int, const TCHAR*, unsigned int, TCHAR*, unsigned int *);
	typedef NSCAPI::errorReturn (*lpNSAPIDecrypt)(unsigned int, const TCHAR*, unsigned int, TCHAR*, unsigned int *);
	typedef NSCAPI::errorReturn (*lpNSAPISetSettingsString)(const TCHAR*, const TCHAR*, const TCHAR*);
	typedef NSCAPI::errorReturn (*lpNSAPISetSettingsInt)(const TCHAR*, const TCHAR*, int);
	typedef NSCAPI::errorReturn (*lpNSAPIWriteSettings)(int);
	typedef NSCAPI::errorReturn (*lpNSAPIReadSettings)(int);
	typedef NSCAPI::errorReturn (*lpNSAPIRehash)(int);
	typedef NSCAPI::errorReturn (*lpNSAPIDescribeCommand)(const TCHAR*,TCHAR*,unsigned int);
	typedef NSCAPI::errorReturn (*lpNSAPIGetAllCommandNames)(arrayBuffer::arrayBuffer*, unsigned int *);
	typedef NSCAPI::errorReturn (*lpNSAPIReleaseAllCommandNamessBuffer)(arrayBuffer::arrayBuffer*, unsigned int *);
	typedef NSCAPI::errorReturn (*lpNSAPIRegisterCommand)(const TCHAR*,const TCHAR*);

	// Helper functions for calling into the core
	std::wstring getApplicationName(void);
	std::wstring getApplicationVersionString(void);
	std::list<std::wstring> getSettingsSection(std::wstring section);
	std::wstring getSettingsString(std::wstring section, std::wstring key, std::wstring defaultValue);
	int getSettingsInt(std::wstring section, std::wstring key, int defaultValue);
	void Message(int msgType, std::wstring file, int line, std::wstring message);
	NSCAPI::nagiosReturn InjectCommandRAW(const TCHAR* command, const unsigned int argLen, TCHAR **argument, TCHAR *returnMessageBuffer, unsigned int returnMessageBufferLen, TCHAR *returnPerfBuffer, unsigned int returnPerfBufferLen);
	NSCAPI::nagiosReturn InjectCommand(const TCHAR* command, const unsigned int argLen, TCHAR **argument, std::wstring & message, std::wstring & perf);
	NSCAPI::nagiosReturn InjectSplitAndCommand(const TCHAR* command, TCHAR* buffer, TCHAR splitChar, std::wstring & message, std::wstring & perf);
	NSCAPI::nagiosReturn InjectSplitAndCommand(const std::wstring command, const std::wstring buffer, TCHAR splitChar, std::wstring & message, std::wstring & perf, bool escape = false);
	void StopService(void);
	std::wstring getBasePath();
	bool logDebug();
	bool checkLogMessages(int type);
	std::wstring Encrypt(std::wstring str, unsigned int algorithm = NSCAPI::xor);
	std::wstring Decrypt(std::wstring str, unsigned int algorithm = NSCAPI::xor);
	NSCAPI::errorReturn SetSettingsString(std::wstring section, std::wstring key, std::wstring value);
	NSCAPI::errorReturn SetSettingsInt(std::wstring section, std::wstring key, int value);
	NSCAPI::errorReturn WriteSettings(int type);
	NSCAPI::errorReturn ReadSettings(int type);
	NSCAPI::errorReturn Rehash(int flag);
	std::list<std::wstring> getAllCommandNames();
	std::wstring describeCommand(std::wstring command);
	void registerCommand(std::wstring command, std::wstring description);
};

namespace NSCModuleWrapper {
	struct module_version {
		int major;
		int minor;
		int revision;
	};

	BOOL wrapDllMain(HANDLE hModule, DWORD ul_reason_for_call);
	HINSTANCE getModule();

	int wrapModuleHelperInit(NSCModuleHelper::lpNSAPILoader f);;
	NSCAPI::errorReturn wrapGetModuleName(TCHAR* buf, unsigned int buflen, std::wstring str);
	NSCAPI::errorReturn wrapGetConfigurationMeta(TCHAR* buf, unsigned int buflen, std::wstring str);
	int wrapLoadModule(bool success);
	NSCAPI::errorReturn wrapGetModuleVersion(int *major, int *minor, int *revision, module_version version);
	NSCAPI::boolReturn wrapHasCommandHandler(bool has);
	NSCAPI::boolReturn wrapHasMessageHandler(bool has);
	int wrapUnloadModule(bool success);
	NSCAPI::nagiosReturn wrapHandleCommand(NSCAPI::nagiosReturn retResult, const std::wstring retMessage, const std::wstring retPerformance, TCHAR *returnBufferMessage, unsigned int returnBufferMessageLen, TCHAR *returnBufferPerf, unsigned int returnBufferPerfLen);
}

//////////////////////////////////////////////////////////////////////////
// Module wrappers (definitions)
#define NSC_WRAPPERS_MAIN() \
	extern "C" int NSModuleHelperInit(NSCModuleHelper::lpNSAPILoader f); \
	extern int NSLoadModule(); \
	extern int NSGetModuleName(TCHAR* buf, int buflen); \
	extern int NSGetModuleDescription(TCHAR* buf, int buflen); \
	extern int NSGetModuleVersion(int *major, int *minor, int *revision); \
	extern NSCAPI::boolReturn NSHasCommandHandler(); \
	extern NSCAPI::boolReturn NSHasMessageHandler(); \
	extern void NSHandleMessage(int msgType, TCHAR* file, int line, TCHAR* message); \
	extern NSCAPI::nagiosReturn NSHandleCommand(const TCHAR* IN_cmd, const unsigned int IN_argsLen, TCHAR **IN_args, \
		TCHAR *OUT_retBufMessage, unsigned int IN_retBufMessageLen, TCHAR *OUT_retBufPerf, unsigned int IN_retBufPerfLen); \
	extern int NSUnloadModule(); \
	extern int NSGetConfigurationMeta(int IN_retBufLen, TCHAR *OUT_retBuf)

#define NSC_WRAPPERS_CLI() \
	extern int NSCommandLineExec(const TCHAR*,const unsigned int,TCHAR**)



#define NSC_LOG_ERROR_STD(msg) NSC_LOG_ERROR(((std::wstring)msg).c_str())
#define NSC_LOG_ERROR(msg) \
	NSCModuleHelper::Message(NSCAPI::error, _T(__FILE__), __LINE__, msg)

#define NSC_LOG_CRITICAL_STD(msg) NSC_LOG_CRITICAL(((std::wstring)msg).c_str())
#define NSC_LOG_CRITICAL(msg) \
	NSCModuleHelper::Message(NSCAPI::critical, _T(__FILE__), __LINE__, msg)

#define NSC_LOG_MESSAGE_STD(msg) NSC_LOG_MESSAGE(((std::wstring)msg).c_str())
#define NSC_LOG_MESSAGE(msg) \
	NSCModuleHelper::Message(NSCAPI::log, _T(__FILE__), __LINE__, msg)

//#define NSC_DEBUG_MSG_STD(msg) NSC_DEBUG_MSG(((std::wstring)msg).c_str())
#define NSC_DEBUG_MSG_STD(msg) NSC_DEBUG_MSG((std::wstring)msg)
#define NSC_DEBUG_MSG(msg) \
	NSCModuleHelper::Message(NSCAPI::debug, _T(__FILE__), __LINE__, msg)

/*
#define NSC_DEBUG_MSG_STD(msg)
#define NSC_DEBUG_MSG(msg)
*/
//////////////////////////////////////////////////////////////////////////
// Message wrappers below this point

#define NSC_WRAPPERS_MAIN_DEF(toObject) \
	extern int NSModuleHelperInit(NSCModuleHelper::lpNSAPILoader f) { \
		try { \
			return NSCModuleWrapper::wrapModuleHelperInit(f); \
		} catch (...) { \
			NSC_LOG_CRITICAL(_T("Unknown exception in: wrapModuleHelperInit(...)")); \
			return NSCAPI::hasFailed; \
		} \
	} \
	extern int NSLoadModule() { \
		try { \
			return NSCModuleWrapper::wrapLoadModule(toObject.loadModule()); \
		} catch (...) { \
			NSC_LOG_CRITICAL(_T("Unknown exception in: wrapLoadModule(...)")); \
			return NSCAPI::hasFailed; \
		} \
	} \
	extern int NSGetModuleName(TCHAR* buf, int buflen) { \
		try { \
			return NSCModuleWrapper::wrapGetModuleName(buf, buflen, toObject.getModuleName()); \
		} catch (...) { \
			NSC_LOG_CRITICAL(_T("Unknown exception in: wrapGetModuleName(...)")); \
			return NSCAPI::hasFailed; \
		} \
	} \
	extern int NSGetModuleDescription(TCHAR* buf, int buflen) { \
		try { \
			return NSCModuleWrapper::wrapGetModuleName(buf, buflen, toObject.getModuleDescription()); \
		} catch (...) { \
			NSC_LOG_CRITICAL(_T("Unknown exception in: wrapGetModuleName(...)")); \
			return NSCAPI::hasFailed; \
		} \
	} \
	extern int NSGetModuleVersion(int *major, int *minor, int *revision) { \
		try { \
			return NSCModuleWrapper::wrapGetModuleVersion(major, minor, revision, toObject.getModuleVersion()); \
		} catch (...) { \
			NSC_LOG_CRITICAL(_T("Unknown exception in: wrapGetModuleVersion(...)")); \
			return NSCAPI::hasFailed; \
		} \
	} \
	extern int NSUnloadModule() { \
		try { \
			return NSCModuleWrapper::wrapUnloadModule(toObject.unloadModule()); \
		} catch (...) { \
			NSC_LOG_CRITICAL(_T("Unknown exception in: wrapGetModuleVersion(...)")); \
			return NSCAPI::hasFailed; \
		} \
	}
#define NSC_WRAPPERS_HANDLE_MSG_DEF(toObject) \
	extern void NSHandleMessage(int msgType, TCHAR* file, int line, TCHAR* message) { \
		try { \
			toObject.handleMessage(msgType, file, line, message); \
		} catch (...) { \
			NSC_LOG_CRITICAL(_T("Unknown exception in: handleMessage(...)")); \
		} \
	} \
	extern NSCAPI::boolReturn NSHasMessageHandler() { \
		try { \
			return NSCModuleWrapper::wrapHasMessageHandler(toObject.hasMessageHandler()); \
		} catch (...) { \
			NSC_LOG_CRITICAL(_T("Unknown exception in: wrapHasMessageHandler(...)")); \
			return NSCAPI::isfalse; \
		} \
	}
#define NSC_WRAPPERS_IGNORE_MSG_DEF() \
	extern void NSHandleMessage(int msgType, TCHAR* file, int line, TCHAR* message) {} \
	extern NSCAPI::boolReturn NSHasMessageHandler() { return NSCAPI::isfalse; }
#define NSC_WRAPPERS_HANDLE_CMD_DEF(toObject) \
	extern NSCAPI::nagiosReturn NSHandleCommand(const TCHAR* IN_cmd, const unsigned int IN_argsLen, TCHAR **IN_args, \
									TCHAR *OUT_retBufMessage, unsigned int IN_retBufMessageLen, TCHAR *OUT_retBufPerf, unsigned int IN_retBufPerfLen) \
	{ \
		try { \
			std::wstring message, perf; \
			NSCAPI::nagiosReturn retCode = toObject.handleCommand(IN_cmd, IN_argsLen, IN_args, message, perf); \
			return NSCModuleWrapper::wrapHandleCommand(retCode, message, perf, OUT_retBufMessage, IN_retBufMessageLen, OUT_retBufPerf, IN_retBufPerfLen); \
		} catch (...) { \
			NSC_LOG_CRITICAL(_T("Unknown exception in: wrapHandleCommand(...)")); \
			return NSCAPI::returnIgnored; \
		} \
	} \
	extern NSCAPI::boolReturn NSHasCommandHandler() { \
		try { \
			return NSCModuleWrapper::wrapHasCommandHandler(toObject.hasCommandHandler()); \
		} catch (...) { \
			NSC_LOG_CRITICAL(_T("Unknown exception in: wrapHasCommandHandler(...)")); \
			return NSCAPI::isfalse; \
		} \
	}
#define NSC_WRAPPERS_IGNORE_CMD_DEF() \
	extern NSCAPI::nagiosReturn NSHandleCommand(const TCHAR* IN_cmd, const unsigned int IN_argsLen, TCHAR **IN_args, \
									TCHAR *OUT_retBufMessage, unsigned int IN_retBufMessageLen, TCHAR *OUT_retBufPerf, unsigned int IN_retBufPerfLen) { \
		return NSCAPI::returnIgnored; \
	} \
	extern NSCAPI::boolReturn NSHasCommandHandler() { return NSCAPI::isfalse; }


#define NSC_WRAPPERS_HANDLE_CONFIGURATION(toObject) \
	extern int NSGetConfigurationMeta(int IN_retBufLen, TCHAR *OUT_retBuf) \
	{ \
		try { \
			return NSCModuleWrapper::wrapGetConfigurationMeta(OUT_retBuf, IN_retBufLen, toObject.getConfigurationMeta()); \
		} catch (...) { \
			NSC_LOG_CRITICAL(_T("Unknown exception in: wrapGetConfigurationMeta(...)")); \
			return NSCAPI::hasFailed; \
		} \
	}

#define NSC_WRAPPERS_CLI_DEF(toObject) \
	extern int NSCommandLineExec(const TCHAR* command,const unsigned int argLen,TCHAR** args) { \
		try { \
			return toObject.commandLineExec(command, argLen, args); \
		} catch (...) { \
			NSC_LOG_CRITICAL(_T("Unknown exception in: commandLineExec(...)")); \
			return NSCAPI::hasFailed; \
		} \
	} \

//////////////////////////////////////////////////////////////////////////
#define MODULE_SETTINGS_START(class, name, description) \
	std::wstring class::getConfigurationMeta() { \
	return (std::wstring)_T("<module name=\"") + name + _T("\" description=\"") + description + _T("\">") \
	_T("<pages>")


#define ADVANCED_PAGE(title) \
	_T("<page title=\"") title _T("\" advanced=\"true\">") \
	_T("<items>")

#define PAGE(title) \
	_T("<page title=\"") title _T("\">") \
	_T("<items>")

#define ITEM_EDIT_TEXT(caption, description) \
	_T("<item type=\"text\" caption=\"") caption _T("\" description=\"") description _T("\"><options>")

#define ITEM_EDIT_OPTIONAL_LIST(caption, description) \
	_T("<item type=\"optional_list\" caption=\"") caption _T("\" description=\"") description _T("\"><options>")

#define ITEM_CHECK_BOOL(caption, description) \
	_T("<item type=\"bool\" caption=\"") caption _T("\" description=\"") description _T("\"><options>")

#define ITEM_MAP_TO(type) \
	_T("</options><mapper type=\"") type _T("\">") \
	_T("<options>")

#define OPTION(key, value) \
	_T("<option key=\"") key _T("\" value=\"") value _T("\"/>")

#define ITEM_END() \
	_T("</options>") \
	_T("</mapper>") \
	_T("</item>")

#define PAGE_END() \
	_T("</items>") \
	_T("</page>")

#define MODULE_SETTINGS_END() \
			_T("</pages>") \
		_T("</module>"); \
	}


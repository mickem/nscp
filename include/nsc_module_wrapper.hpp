#pragma once

#ifdef WIN32
#include <windows.h>
#endif


namespace NSCModuleWrapper {
	struct module_version {
		int major;
		int minor;
		int revision;
	};
#ifdef WIN32
	int wrapDllMain(HANDLE hModule, DWORD ul_reason_for_call);
	HINSTANCE getModule();
#endif
	int wrapModuleHelperInit(NSCModuleHelper::lpNSAPILoader f);;
	NSCAPI::errorReturn wrapGetModuleName(wchar_t* buf, unsigned int buflen, std::wstring str);
	NSCAPI::errorReturn wrapGetConfigurationMeta(wchar_t* buf, unsigned int buflen, std::wstring str);
	int wrapLoadModule(bool success);
	NSCAPI::errorReturn wrapGetModuleVersion(int *major, int *minor, int *revision, module_version version);
	NSCAPI::boolReturn wrapHasCommandHandler(bool has);
	NSCAPI::boolReturn wrapHasMessageHandler(bool has);
	int wrapUnloadModule(bool success);
	NSCAPI::nagiosReturn wrapHandleCommand(NSCAPI::nagiosReturn retResult, const std::wstring retMessage, const std::wstring retPerformance, wchar_t *returnBufferMessage, unsigned int returnBufferMessageLen, wchar_t *returnBufferPerf, unsigned int returnBufferPerfLen);
}

//////////////////////////////////////////////////////////////////////////
// Module wrappers (definitions)
#define NSC_WRAPPERS_MAIN() \
	extern "C" int NSModuleHelperInit(NSCModuleHelper::lpNSAPILoader f); \
	extern int NSLoadModule(int mode); \
	extern int NSGetModuleName(wchar_t* buf, int buflen); \
	extern int NSGetModuleDescription(wchar_t* buf, int buflen); \
	extern int NSGetModuleVersion(int *major, int *minor, int *revision); \
	extern NSCAPI::boolReturn NSHasCommandHandler(); \
	extern NSCAPI::boolReturn NSHasMessageHandler(); \
	extern void NSHandleMessage(int msgType, wchar_t* file, int line, wchar_t* message); \
	extern NSCAPI::nagiosReturn NSHandleCommand(const wchar_t* IN_cmd, const unsigned int IN_argsLen, wchar_t **IN_args, \
		wchar_t *OUT_retBufMessage, unsigned int IN_retBufMessageLen, wchar_t *OUT_retBufPerf, unsigned int IN_retBufPerfLen); \
	extern int NSUnloadModule(); \
	extern int NSGetConfigurationMeta(int IN_retBufLen, wchar_t *OUT_retBuf)

#define NSC_WRAPPERS_CLI() \
	extern int NSCommandLineExec(const wchar_t*,const unsigned int,wchar_t**)

#ifdef DEBUG
#define NSC_LOG_ERROR_STD_C(msg) NSC_LOG_ERROR(((std::wstring)msg).c_str())
#define NSC_LOG_ERROR_C(msg) { \
	NSCModuleHelper::Message(NSCAPI::error, _T(__FILE__), __LINE__, msg) \
	std::wcerr << msg << std::endl; }
#else
#define NSC_LOG_ERROR_STD_C(msg) NSC_LOG_ERROR_STD(msg)
#define NSC_LOG_ERROR_C(msg) NSC_LOG_ERROR(msg)
#endif


#define NSC_LOG_ERROR_STD(msg) NSC_LOG_ERROR(((std::wstring)msg).c_str())
#define NSC_LOG_ERROR(msg) \
	NSCModuleHelper::Message(NSCAPI::error, __FILEW__, __LINE__, msg)

#define NSC_LOG_CRITICAL_STD(msg) NSC_LOG_CRITICAL(((std::wstring)msg).c_str())
#define NSC_LOG_CRITICAL(msg) \
	NSCModuleHelper::Message(NSCAPI::critical, __FILEW__, __LINE__, msg)

#define NSC_LOG_MESSAGE_STD(msg) NSC_LOG_MESSAGE(((std::wstring)msg).c_str())
#define NSC_LOG_MESSAGE(msg) \
	NSCModuleHelper::Message(NSCAPI::log, __FILEW__, __LINE__, msg)

//#define NSC_DEBUG_MSG_STD(msg) NSC_DEBUG_MSG(((std::wstring)msg).c_str())
#define NSC_DEBUG_MSG_STD(msg) NSC_DEBUG_MSG((std::wstring)msg)
#define NSC_DEBUG_MSG(msg) \
	NSCModuleHelper::Message(NSCAPI::debug, __FILEW__, __LINE__, msg)

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
	extern int NSLoadModule(int mode) { \
		try { \
			return NSCModuleWrapper::wrapLoadModule(toObject.loadModule(mode)); \
		} catch (NSCModuleHelper::NSCMHExcpetion e) { \
			NSC_LOG_CRITICAL(_T("NSCMHE in: wrapLoadModule: " + e.msg_)); \
			return NSCAPI::hasFailed; \
		} catch (...) { \
			NSC_LOG_CRITICAL(_T("Unknown exception in: wrapLoadModule(...)")); \
			return NSCAPI::hasFailed; \
		} \
	} \
	extern int NSGetModuleName(wchar_t* buf, int buflen) { \
		try { \
			return NSCModuleWrapper::wrapGetModuleName(buf, buflen, toObject.getModuleName()); \
		} catch (...) { \
			NSC_LOG_CRITICAL(_T("Unknown exception in: wrapGetModuleName(...)")); \
			return NSCAPI::hasFailed; \
		} \
	} \
	extern int NSGetModuleDescription(wchar_t* buf, int buflen) { \
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
	extern void NSHandleMessage(int msgType, wchar_t* file, int line, wchar_t* message) { \
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
	extern void NSHandleMessage(int msgType, wchar_t* file, int line, wchar_t* message) {} \
	extern NSCAPI::boolReturn NSHasMessageHandler() { return NSCAPI::isfalse; }
#define NSC_WRAPPERS_HANDLE_CMD_DEF(toObject) \
	extern NSCAPI::nagiosReturn NSHandleCommand(const wchar_t* IN_cmd, const unsigned int IN_argsLen, wchar_t **IN_args, \
									wchar_t *OUT_retBufMessage, unsigned int IN_retBufMessageLen, wchar_t *OUT_retBufPerf, unsigned int IN_retBufPerfLen) \
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
	extern NSCAPI::nagiosReturn NSHandleCommand(const wchar_t* IN_cmd, const unsigned int IN_argsLen, wchar_t **IN_args, \
									wchar_t *OUT_retBufMessage, unsigned int IN_retBufMessageLen, wchar_t *OUT_retBufPerf, unsigned int IN_retBufPerfLen) { \
		return NSCAPI::returnIgnored; \
	} \
	extern NSCAPI::boolReturn NSHasCommandHandler() { return NSCAPI::isfalse; }


#define NSC_WRAPPERS_HANDLE_CONFIGURATION(toObject) \
	extern int NSGetConfigurationMeta(int IN_retBufLen, wchar_t *OUT_retBuf) \
	{ \
		try { \
			return NSCModuleWrapper::wrapGetConfigurationMeta(OUT_retBuf, IN_retBufLen, toObject.getConfigurationMeta()); \
		} catch (...) { \
			NSC_LOG_CRITICAL(_T("Unknown exception in: wrapGetConfigurationMeta(...)")); \
			return NSCAPI::hasFailed; \
		} \
	}

#define NSC_WRAPPERS_CLI_DEF(toObject) \
	extern int NSCommandLineExec(const wchar_t* command,const unsigned int argLen,wchar_t** args) { \
		try { \
			return toObject.commandLineExec(command, argLen, args); \
		} catch (...) { \
			NSC_LOG_CRITICAL(_T("Unknown exception in: commandLineExec(...)")); \
			std::wcerr << _T("Unknown exception in: commandLineExec(...)") << std::endl; \
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

#pragma once
#include <unicode_char.hpp>

//////////////////////////////////////////////////////////////////////////
// Module wrappers (definitions)
#define NSC_WRAPPERS_MAIN() \
	extern "C" int NSModuleHelperInit(unsigned int id, nscapi::core_api::lpNSAPILoader f); \
	extern "C" int NSLoadModule(); \
	extern "C" int NSLoadModuleEx(const wchar_t alias, int mode); \
	extern "C" void NSDeleteBuffer(char**buffer); \
	extern "C" int NSGetModuleName(wchar_t* buf, int buflen); \
	extern "C" int NSGetModuleDescription(wchar_t* buf, int buflen); \
	extern "C" int NSGetModuleVersion(int *major, int *minor, int *revision); \
	extern "C" NSCAPI::boolReturn NSHasCommandHandler(); \
	extern "C" NSCAPI::boolReturn NSHasMessageHandler(); \
	extern "C" void NSHandleMessage(const char* data); \
	extern "C" NSCAPI::nagiosReturn NSHandleCommand(const wchar_t* command, const char* request_buffer, const unsigned int request_buffer_len, char** reply_buffer, unsigned int *reply_buffer_len); \
	extern "C" int NSUnloadModule();


#define NSC_WRAPPERS_CLI() \
	extern "C" int NSCommandLineExec(const unsigned int,wchar_t**);

#define NSC_WRAPPERS_CHANNELS() \
	extern "C" int NSHasNotificationHandler(); \
	extern "C" int NSHandleNotification(const wchar_t*, const wchar_t*, NSCAPI::nagiosReturn, const char*, unsigned int);

//////////////////////////////////////////////////////////////////////////
// Logging calls for the core wrapper 
/*
#define NSC_LOG_ERROR_STD(msg) NSC_LOG_ERROR(((std::wstring)msg).c_str())
#define NSC_LOG_ERROR(msg) NSC_ANY_MSG(msg,NSCAPI::error)

#define NSC_LOG_CRITICAL_STD(msg) NSC_LOG_CRITICAL(((std::wstring)msg).c_str())
#define NSC_LOG_CRITICAL(msg) NSC_ANY_MSG(msg,NSCAPI::critical)

#define NSC_LOG_MESSAGE_STD(msg) NSC_LOG_MESSAGE(((std::wstring)msg).c_str())
#define NSC_LOG_MESSAGE(msg) NSC_ANY_MSG(msg,NSCAPI::log)

#define NSC_DEBUG_MSG_STD(msg) NSC_DEBUG_MSG((std::wstring)msg)
#define NSC_DEBUG_MSG(msg) NSC_ANY_MSG(msg,NSCAPI::debug)

#define NSC_ANY_MSG(msg, type) GET_CORE()->Message(type, __FILE__, __LINE__, msg)
*/
#define NSC_LOG_ERROR_STD(msg) 
#define NSC_LOG_ERROR(msg) 

#define NSC_LOG_CRITICAL_STD(msg) 
#define NSC_LOG_CRITICAL(msg) 

#define NSC_LOG_MESSAGE_STD(msg) 
#define NSC_LOG_MESSAGE(msg) 

#define NSC_DEBUG_MSG_STD(msg)
#define NSC_DEBUG_MSG(msg)

#define NSC_ANY_MSG(msg, type)


//////////////////////////////////////////////////////////////////////////
// Message wrappers below this point

#ifdef _WIN32
#define NSC_WRAP_DLL() \
	BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved) { GET_PLUGIN()->wrapDllMain(hModule, ul_reason_for_call); return TRUE; } \
	nscapi::helper_singleton* nscapi::plugin_singleton = new nscapi::helper_singleton();
#else
#define NSC_WRAP_DLL() \
	nscapi::helper_singleton* nscapi::plugin_singleton = new nscapi::helper_singleton();
#endif

#define NSC_WRAPPERS_MAIN_DEF(toObject) \
	extern int NSModuleHelperInit(unsigned int id, nscapi::core_api::lpNSAPILoader f) { \
		try { \
			return GET_PLUGIN()->wrapModuleHelperInit(id, f); \
		} catch (...) { \
			NSC_LOG_CRITICAL(_T("Unknown exception in: wrapModuleHelperInit(...)")); \
			return NSCAPI::hasFailed; \
		} \
	} \
	extern int NSLoadModuleEx(wchar_t* alias, int mode) { \
	try { \
	return GET_PLUGIN()->wrapLoadModule(toObject.loadModuleEx(alias, mode)); \
		} catch (nscapi::nscapi_exception e) { \
		NSC_LOG_CRITICAL(_T("NSCMHE in: wrapLoadModule: " + e.msg_)); \
		return NSCAPI::hasFailed; \
		} catch (...) { \
		NSC_LOG_CRITICAL(_T("Unknown exception in: wrapLoadModule(...)")); \
		return NSCAPI::hasFailed; \
		} \
	} \
	extern int NSLoadModule() { \
	try { \
	return GET_PLUGIN()->wrapLoadModule(toObject.loadModule()); \
		} catch (nscapi::nscapi_exception e) { \
		NSC_LOG_CRITICAL(_T("NSCMHE in: wrapLoadModule: " + e.msg_)); \
		return NSCAPI::hasFailed; \
		} catch (...) { \
		NSC_LOG_CRITICAL(_T("Unknown exception in: wrapLoadModule(...)")); \
		return NSCAPI::hasFailed; \
		} \
	} \
	extern int NSGetModuleName(wchar_t* buf, int buflen) { \
		try { \
			return GET_PLUGIN()->wrapGetModuleName(buf, buflen, toObject.getModuleName()); \
		} catch (...) { \
			NSC_LOG_CRITICAL(_T("Unknown exception in: wrapGetModuleName(...)")); \
			return NSCAPI::hasFailed; \
		} \
	} \
	extern int NSGetModuleDescription(wchar_t* buf, int buflen) { \
		try { \
			return GET_PLUGIN()->wrapGetModuleName(buf, buflen, toObject.getModuleDescription()); \
		} catch (...) { \
			NSC_LOG_CRITICAL(_T("Unknown exception in: wrapGetModuleName(...)")); \
			return NSCAPI::hasFailed; \
		} \
	} \
	extern int NSGetModuleVersion(int *major, int *minor, int *revision) { \
		try { \
			return GET_PLUGIN()->wrapGetModuleVersion(major, minor, revision, toObject.getModuleVersion()); \
		} catch (...) { \
			NSC_LOG_CRITICAL(_T("Unknown exception in: wrapGetModuleVersion(...)")); \
			return NSCAPI::hasFailed; \
		} \
	} \
	extern int NSUnloadModule() { \
		try { \
			return GET_PLUGIN()->wrapUnloadModule(toObject.unloadModule()); \
		} catch (...) { \
			NSC_LOG_CRITICAL(_T("Unknown exception in: wrapGetModuleVersion(...)")); \
			return NSCAPI::hasFailed; \
		} \
	} \
	extern void NSDeleteBuffer(char**buffer) { \
		try { \
			GET_PLUGIN()->wrapDeleteBuffer(buffer); \
		} catch (...) { \
			NSC_LOG_CRITICAL(_T("Unknown exception in: wrapModuleHelperInit(...)")); \
		} \
	}
#define NSC_WRAPPERS_HANDLE_MSG_DEF(toObject) \
	extern void NSHandleMessage(const char* data) { \
		try { \
			toObject.handleMessageRAW(data); \
		} catch (...) { \
			NSC_LOG_CRITICAL(_T("Unknown exception in: handleMessage(...)")); \
		} \
	} \
	extern NSCAPI::boolReturn NSHasMessageHandler() { \
		try { \
			return GET_PLUGIN()->wrapHasMessageHandler(toObject.hasMessageHandler()); \
		} catch (...) { \
			NSC_LOG_CRITICAL(_T("Unknown exception in: wrapHasMessageHandler(...)")); \
			return NSCAPI::isfalse; \
		} \
	}
#define NSC_WRAPPERS_IGNORE_MSG_DEF() \
	extern void NSHandleMessage(const char* data) {} \
	extern NSCAPI::boolReturn NSHasMessageHandler() { return NSCAPI::isfalse; }
#define NSC_WRAPPERS_HANDLE_CMD_DEF(toObject) \
	extern NSCAPI::nagiosReturn NSHandleCommand(const wchar_t* command, const char* request_buffer, const unsigned int request_buffer_len, char** reply_buffer, unsigned int *reply_buffer_len) \
	{ \
	try { \
	std::string request(request_buffer, request_buffer_len), reply; \
	NSCAPI::nagiosReturn retCode = (&toObject)->handleRAWCommand(command, request, reply); \
	return GET_PLUGIN()->wrapHandleCommand(retCode, reply, reply_buffer, reply_buffer_len); \
		} catch (...) { \
		NSC_LOG_CRITICAL(_T("Unknown exception in: wrapHandleCommand(...)")); \
		return NSCAPI::returnIgnored; \
		} \
	} \
	extern NSCAPI::boolReturn NSHasCommandHandler() { \
	try { \
	return GET_PLUGIN()->wrapHasCommandHandler(toObject.hasCommandHandler()); \
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
#define NSC_WRAPPERS_IGNORE_NOTIFICATION_DEF() \
	extern void NSHandleNotification(const wchar_t*, const wchar_t*, NSCAPI::nagiosReturn, const char*, unsigned int) {} \
	extern NSCAPI::boolReturn NSHasNotificationHandler() { return NSCAPI::isfalse; }
#define NSC_WRAPPERS_HANDLE_NOTIFICATION_DEF(toObject) \
	extern NSCAPI::nagiosReturn NSHandleNotification(const wchar_t* channel, const wchar_t* command, NSCAPI::nagiosReturn code, const char* result_buffer, unsigned int result_buffer_len) \
	{ \
		try { \
			std::string result(result_buffer, result_buffer_len); \
			return GET_PLUGIN()->wrapHandleNotification((&toObject)->handleRAWNotification(channel, command, code, result)); \
		} catch (...) { \
			NSC_LOG_CRITICAL(_T("Unknown exception in: wrapHasNotificationHandler(...)")); \
			return NSCAPI::returnIgnored; \
		} \
	} \
	extern NSCAPI::boolReturn NSHasNotificationHandler() { \
		try { \
			return GET_PLUGIN()->wrapHasNotificationHandler(toObject.hasNotificationHandler()); \
		} catch (...) { \
			NSC_LOG_CRITICAL(_T("Unknown exception in: wrapHasNotificationHandler(...)")); \
			return NSCAPI::isfalse; \
		} \
	}


#define NSC_WRAPPERS_CLI_DEF(toObject) \
	extern int NSCommandLineExec(const unsigned int argLen,wchar_t** args) { \
		try { \
			return toObject.commandLineExec(argLen, args); \
		} catch (...) { \
			NSC_LOG_CRITICAL(_T("Unknown exception in: commandLineExec(...)")); \
			std::wcerr << _T("Unknown exception in: commandLineExec(...)") << std::endl; \
			return NSCAPI::hasFailed; \
		} \
	} \



#define SETTINGS_MAKE_NAME(key) \
	std::wstring(setting_keys::key ## _PATH + _T(".") + setting_keys::key)

#define SETTINGS_GET_STRING(key) \
	GET_CORE()->getSettingsString(setting_keys::key ## _PATH, setting_keys::key, setting_keys::key ## _DEFAULT)
#define SETTINGS_GET_INT(key) \
	GET_CORE()->getSettingsInt(setting_keys::key ## _PATH, setting_keys::key, setting_keys::key ## _DEFAULT)
#define SETTINGS_GET_BOOL(key) \
	GET_CORE()->getSettingsInt(setting_keys::key ## _PATH, setting_keys::key, setting_keys::key ## _DEFAULT)

#define SETTINGS_GET_STRING_FALLBACK(key, fallback) \
	GET_CORE()->getSettingsString(setting_keys::key ## _PATH, setting_keys::key, GET_CORE()->getSettingsString(setting_keys::fallback ## _PATH, setting_keys::fallback, setting_keys::fallback ## _DEFAULT))
#define SETTINGS_GET_INT_FALLBACK(key, fallback) \
	GET_CORE()->getSettingsInt(setting_keys::key ## _PATH, setting_keys::key, GET_CORE()->getSettingsInt(setting_keys::fallback ## _PATH, setting_keys::fallback, setting_keys::fallback ## _DEFAULT))
#define SETTINGS_GET_BOOL_FALLBACK(key, fallback) \
	GET_CORE()->getSettingsInt(setting_keys::key ## _PATH, setting_keys::key, GET_CORE()->getSettingsInt(setting_keys::fallback ## _PATH, setting_keys::fallback, setting_keys::fallback ## _DEFAULT))

#define SETTINGS_REG_KEY_S(key) \
	GET_CORE()->settings_register_key(setting_keys::key ## _PATH, setting_keys::key, NSCAPI::key_string, setting_keys::key ## _TITLE, setting_keys::key ## _DESC, setting_keys::key ## _DEFAULT, setting_keys::key ## _ADVANCED);
#define SETTINGS_REG_KEY_I(key) \
	GET_CORE()->settings_register_key(setting_keys::key ## _PATH, setting_keys::key, NSCAPI::key_integer, setting_keys::key ## _TITLE, setting_keys::key ## _DESC, boost::lexical_cast<std::wstring>(setting_keys::key ## _DEFAULT), setting_keys::key ## _ADVANCED);
#define SETTINGS_REG_KEY_B(key) \
	GET_CORE()->settings_register_key(setting_keys::key ## _PATH, setting_keys::key, NSCAPI::key_integer, setting_keys::key ## _TITLE, setting_keys::key ## _DESC, setting_keys::key ## _DEFAULT==1?_T("1"):_T("0"), setting_keys::key ## _ADVANCED);
#define SETTINGS_REG_PATH(key) \
	GET_CORE()->settings_register_path(setting_keys::key ## _PATH, setting_keys::key ## _TITLE, setting_keys::key ## _DESC, setting_keys::key ## _ADVANCED);

#define GET_CORE() nscapi::plugin_singleton->get_core()
#define GET_PLUGIN() nscapi::plugin_singleton->get_plugin()
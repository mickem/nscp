#pragma once

#include <string>
#include <list>
#include <NSCAPI.h>
#include <iostream>
#include <charEx.h>
#include <arrayBuffer.h>

namespace NSCHelper
{
#ifdef DEBUG
	NSCAPI::nagiosReturn wrapReturnString(char *buffer, unsigned int bufLen, std::string str, NSCAPI::nagiosReturn defaultReturnCode);
	NSCAPI::errorReturn wrapReturnString(char *buffer, unsigned int bufLen, std::string str, NSCAPI::errorReturn defaultReturnCode);
#else
	int wrapReturnString(char *buffer, unsigned int bufLen, std::string str, int defaultReturnCode);
#endif
	std::string translateMessageType(NSCAPI::messageTypes msgType);
	std::string translateReturn(NSCAPI::nagiosReturn returnCode);

	/*
	/ * **************************************************************************
	* max_state(STATE_x, STATE_y)
	* compares STATE_x to  STATE_y and returns result based on the following
	* STATE_UNKNOWN < STATE_OK < STATE_WARNING < STATE_CRITICAL
	*
	* Note that numerically the above does not hold
	**************************************************************************** /

	int
		max_state (int a, int b)
	{
		if (a == STATE_CRITICAL || b == STATE_CRITICAL)
			return STATE_CRITICAL;
		else if (a == STATE_WARNING || b == STATE_WARNING)
			return STATE_WARNING;
		else if (a == STATE_OK || b == STATE_OK)
			return STATE_OK;
		else if (a == STATE_UNKNOWN || b == STATE_UNKNOWN)
			return STATE_UNKNOWN;
		else if (a == STATE_DEPENDENT || b == STATE_DEPENDENT)
			return STATE_DEPENDENT;
		else
			return max (a, b);
	}
	@bug Use this scheme instead!!
*/


	inline void escalteReturnCode(NSCAPI::nagiosReturn &currentReturnCode, NSCAPI::nagiosReturn newReturnCode) {
		if (newReturnCode == NSCAPI::returnCRIT)
			currentReturnCode = NSCAPI::returnCRIT;
		else if ((newReturnCode == NSCAPI::returnWARN) && (currentReturnCode != NSCAPI::returnCRIT) )
			currentReturnCode = NSCAPI::returnWARN;
		else if ((newReturnCode == NSCAPI::returnUNKNOWN) 
			&& (currentReturnCode != NSCAPI::returnCRIT) 
			&& (currentReturnCode != NSCAPI::returnWARN) )
			currentReturnCode = NSCAPI::returnUNKNOWN;
	}
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
		std::string msg_;
		NSCMHExcpetion(std::string msg) : msg_(msg) {}
	};
	// Types for the Callbacks into the main program
	typedef NSCAPI::errorReturn (*lpNSAPIGetBasePath)(char*,unsigned int);
	typedef NSCAPI::errorReturn (*lpNSAPIGetApplicationName)(char*,unsigned int);
	typedef NSCAPI::errorReturn (*lpNSAPIGetApplicationVersionStr)(char*,unsigned int);
	typedef NSCAPI::errorReturn (*lpNSAPIGetSettingsString)(const char*,const char*,const char*,char*,unsigned int);
	typedef NSCAPI::errorReturn (*lpNSAPIGetSettingsInt)(const char*, const char*, int);
	typedef void (*lpNSAPIMessage)(int, const char*, const int, const char*);
	typedef NSCAPI::errorReturn (*lpNSAPIStopServer)(void);
	typedef NSCAPI::nagiosReturn (*lpNSAPIInject)(const char*, const unsigned int, char **, char *, unsigned int, char *, unsigned int);
	typedef LPVOID (*lpNSAPILoader)(char*);

	// Helper functions for calling into the core
	std::string getApplicationName(void);
	std::string getApplicationVersionString(void);
	std::string getSettingsString(std::string section, std::string key, std::string defaultValue);
	int getSettingsInt(std::string section, std::string key, int defaultValue);
	void Message(int msgType, std::string file, int line, std::string message);
	NSCAPI::nagiosReturn InjectCommandRAW(const char* command, const unsigned int argLen, char **argument, char *returnMessageBuffer, unsigned int returnMessageBufferLen, char *returnPerfBuffer, unsigned int returnPerfBufferLen);
	NSCAPI::nagiosReturn InjectCommand(const char* command, const unsigned int argLen, char **argument, std::string & message, std::string & perf);
	NSCAPI::nagiosReturn InjectSplitAndCommand(const char* command, char* buffer, char splitChar, std::string & message, std::string & perf);
	NSCAPI::nagiosReturn InjectSplitAndCommand(const std::string command, const std::string buffer, char splitChar, std::string & message, std::string & perf);
	void StopService(void);
	std::string getBasePath();
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
	NSCAPI::errorReturn wrapGetModuleName(char* buf, unsigned int buflen, std::string str);
	int wrapLoadModule(bool success);
	NSCAPI::errorReturn wrapGetModuleVersion(int *major, int *minor, int *revision, module_version version);
	NSCAPI::boolReturn wrapHasCommandHandler(bool has);
	NSCAPI::boolReturn wrapHasMessageHandler(bool has);
	int wrapUnloadModule(bool success);
	NSCAPI::nagiosReturn wrapHandleCommand(NSCAPI::nagiosReturn retResult, const std::string retMessage, const std::string retPerformance, char *returnBufferMessage, unsigned int returnBufferMessageLen, char *returnBufferPerf, unsigned int returnBufferPerfLen);
}

//////////////////////////////////////////////////////////////////////////
// Module wrappers (definitions)
#define NSC_WRAPPERS_MAIN() \
	extern "C" int NSModuleHelperInit(NSCModuleHelper::lpNSAPILoader f); \
	extern int NSLoadModule(); \
	extern int NSGetModuleName(char* buf, int buflen); \
	extern int NSGetModuleVersion(int *major, int *minor, int *revision); \
	extern NSCAPI::boolReturn NSHasCommandHandler(); \
	extern NSCAPI::boolReturn NSHasMessageHandler(); \
	extern void NSHandleMessage(int msgType, char* file, int line, char* message); \
	extern NSCAPI::nagiosReturn NSHandleCommand(const char* IN_cmd, const unsigned int IN_argsLen, char **IN_args, \
		char *OUT_retBufMessage, unsigned int IN_retBufMessageLen, char *OUT_retBufPerf, unsigned int IN_retBufPerfLen); \
	extern int NSUnloadModule();



#define NSC_LOG_ERROR_STD(msg) NSC_LOG_ERROR(((std::string)msg).c_str())
#define NSC_LOG_ERROR(msg) \
	NSCModuleHelper::Message(NSCAPI::error, __FILE__, __LINE__, msg)

#define NSC_LOG_CRITICAL_STD(msg) NSC_LOG_CRITICAL(((std::string)msg).c_str())
#define NSC_LOG_CRITICAL(msg) \
	NSCModuleHelper::Message(NSCAPI::critical, __FILE__, __LINE__, msg)

#define NSC_LOG_MESSAGE_STD(msg) NSC_LOG_MESSAGE(((std::string)msg).c_str())
#define NSC_LOG_MESSAGE(msg) \
	NSCModuleHelper::Message(NSCAPI::log, __FILE__, __LINE__, msg)

#define NSC_DEBUG_MSG_STD(msg) NSC_DEBUG_MSG(((std::string)msg).c_str())
#define NSC_DEBUG_MSG(msg) \
	NSCModuleHelper::Message(NSCAPI::debug, __FILE__, __LINE__, msg)

//////////////////////////////////////////////////////////////////////////
// Message wrappers below this point

#define NSC_WRAPPERS_MAIN_DEF(toObject) \
	extern int NSModuleHelperInit(NSCModuleHelper::lpNSAPILoader f) { \
		return NSCModuleWrapper::wrapModuleHelperInit(f); \
	} \
	extern int NSLoadModule() { \
		return NSCModuleWrapper::wrapLoadModule(toObject.loadModule()); \
	} \
	extern int NSGetModuleName(char* buf, int buflen) { \
		return NSCModuleWrapper::wrapGetModuleName(buf, buflen, toObject.getModuleName()); \
	} \
	extern int NSGetModuleVersion(int *major, int *minor, int *revision) { \
		return NSCModuleWrapper::wrapGetModuleVersion(major, minor, revision, toObject.getModuleVersion()); \
	} \
	extern int NSUnloadModule() { \
		return NSCModuleWrapper::wrapUnloadModule(toObject.unloadModule()); \
	}
#define NSC_WRAPPERS_HANDLE_MSG_DEF(toObject) \
	extern void NSHandleMessage(int msgType, char* file, int line, char* message) { \
		toObject.handleMessage(msgType, file, line, message); \
	} \
	extern NSCAPI::boolReturn NSHasMessageHandler() { \
		return NSCModuleWrapper::wrapHasMessageHandler(toObject.hasMessageHandler()); \
	}
#define NSC_WRAPPERS_IGNORE_MSG_DEF() \
	extern void NSHandleMessage(int msgType, char* file, int line, char* message) {} \
	extern NSCAPI::boolReturn NSHasMessageHandler() { return NSCAPI::isfalse; }
#define NSC_WRAPPERS_HANDLE_CMD_DEF(toObject) \
	extern NSCAPI::nagiosReturn NSHandleCommand(const char* IN_cmd, const unsigned int IN_argsLen, char **IN_args, \
									char *OUT_retBufMessage, unsigned int IN_retBufMessageLen, char *OUT_retBufPerf, unsigned int IN_retBufPerfLen) \
	{ \
		std::string message, perf; \
		NSCAPI::nagiosReturn retCode = toObject.handleCommand(IN_cmd, IN_argsLen, IN_args, message, perf); \
		return NSCModuleWrapper::wrapHandleCommand(retCode, message, perf, OUT_retBufMessage, IN_retBufMessageLen, OUT_retBufPerf, IN_retBufPerfLen); \
	} \
	extern NSCAPI::boolReturn NSHasCommandHandler() { \
		return NSCModuleWrapper::wrapHasCommandHandler(toObject.hasCommandHandler()); \
	}
#define NSC_WRAPPERS_IGNORE_CMD_DEF() \
	extern NSCAPI::nagiosReturn NSHandleCommand(const char* IN_cmd, const unsigned int IN_argsLen, char **IN_args, \
									char *OUT_retBufMessage, unsigned int IN_retBufMessageLen, char *OUT_retBufPerf, unsigned int IN_retBufPerfLen) { \
		return NSCAPI::returnIgnored; \
	} \
	extern NSCAPI::boolReturn NSHasCommandHandler() { return NSCAPI::isfalse; }

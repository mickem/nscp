#pragma once

#include <string>
#include <list>
#include <NSCAPI.h>
#include <iostream>
#include <charEx.h>

namespace NSCHelper
{
	int wrapReturnString(char *buffer, unsigned int bufLen, std::string str, int defaultReturnCode = NSCAPI::success);

	std::list<std::string> arrayBuffer2list(const unsigned int argLen, char **argument);
	char ** list2arrayBuffer(const std::list<std::string> lst, unsigned int &argLen);
	char ** split2arrayBuffer(const char* buffer, char splitChar, unsigned int &argLen);
	std::string arrayBuffer2string(char **argument, const unsigned int argLen, std::string join);
	char ** createEmptyArrayBuffer(unsigned int &argLen);
	void destroyArrayBuffer(char **argument, const unsigned int argLen);

	std::string translateMessageType(NSCAPI::messageTypes msgType);
	std::string translateReturn(NSCAPI::returnCodes returnCode);
	inline std::string returnNSCP(NSCAPI::returnCodes returnCode, std::string str) {
		return translateReturn(returnCode) + "&" + str;
	}

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


	inline void escalteReturnCode(NSCAPI::returnCodes &currentReturnCode, NSCAPI::returnCodes newReturnCode) {
		if (newReturnCode == NSCAPI::returnCRIT)
			currentReturnCode = NSCAPI::returnCRIT;
		else if ((newReturnCode == NSCAPI::returnWARN) && (currentReturnCode != NSCAPI::returnCRIT) )
			currentReturnCode = NSCAPI::returnWARN;
		else if ((newReturnCode == NSCAPI::returnUNKNOWN) 
			&& (currentReturnCode != NSCAPI::returnCRIT) 
			&& (currentReturnCode != NSCAPI::returnWARN) )
			currentReturnCode = NSCAPI::returnUNKNOWN;
	}
	inline void escalteReturnCodeToCRIT(NSCAPI::returnCodes &currentReturnCode) {
		currentReturnCode = NSCAPI::returnCRIT;
	}
	inline void escalteReturnCodeToWARN(NSCAPI::returnCodes &currentReturnCode) {
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
	typedef int (*lpNSAPIGetBasePath)(char*,unsigned int);
	typedef int (*lpNSAPIGetApplicationName)(char*,unsigned int);
	typedef int (*lpNSAPIGetApplicationVersionStr)(char*,unsigned int);
	typedef int (*lpNSAPIGetSettingsString)(const char*,const char*,const char*,char*,unsigned int);
	typedef int (*lpNSAPIGetSettingsInt)(const char*, const char*, int);
	typedef void (*lpNSAPIMessage)(int, const char*, const int, const char*);
	typedef int (*lpNSAPIStopServer)(void);
	typedef int (*lpNSAPIInject)(const char*, const unsigned int, char **, char *, unsigned int );
	typedef LPVOID (*lpNSAPILoader)(char*);

	// Helper functions for calling into the core
	std::string getApplicationName(void);
	std::string getApplicationVersionString(void);
	std::string getSettingsString(std::string section, std::string key, std::string defaultValue);
	int getSettingsInt(std::string section, std::string key, int defaultValue);
	void Message(int msgType, std::string file, int line, std::string message);
	int InjectCommandRAW(const char* command, const unsigned int argLen, char **argument, char *returnBuffer, unsigned int returnBufferLen);
	std::string InjectCommand(const char* command, const unsigned int argLen, char **argument);
	std::string InjectSplitAndCommand(const char* command, char* buffer, char splitChar = '&');
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
	int wrapGetModuleName(char* buf, unsigned int buflen, std::string str);
	int wrapLoadModule(bool success);
	int wrapGetModuleVersion(int *major, int *minor, int *revision, module_version version);
	int wrapHasCommandHandler(bool has);
	int wrapHasMessageHandler(bool has);
	int wrapUnloadModule(bool success);
	int wrapHandleCommand(const std::string retStr, char *returnBuffer, unsigned int returnBufferLen);
}

//////////////////////////////////////////////////////////////////////////
// Module wrappers (definitions)
#define NSC_WRAPPERS_MAIN() \
	extern "C" int NSModuleHelperInit(NSCModuleHelper::lpNSAPILoader f); \
	extern int NSLoadModule(); \
	extern int NSGetModuleName(char* buf, int buflen); \
	extern int NSGetModuleVersion(int *major, int *minor, int *revision); \
	extern int NSHasCommandHandler(); \
	extern int NSHasMessageHandler(); \
	extern void NSHandleMessage(int msgType, char* file, int line, char* message); \
	extern int NSHandleCommand(const char* command, const unsigned int argLen, char **argument, char *returnBuffer, unsigned int returnBufferLen); \
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
	extern int NSHasMessageHandler() { \
		return NSCModuleWrapper::wrapHasMessageHandler(toObject.hasMessageHandler()); \
	}
#define NSC_WRAPPERS_IGNORE_MSG_DEF() \
	extern void NSHandleMessage(int msgType, char* file, int line, char* message) {} \
	extern int NSHasMessageHandler() { return NSCAPI::isfalse; }
#define NSC_WRAPPERS_HANDLE_CMD_DEF(toObject) \
	extern int NSHandleCommand(const char* command, const unsigned int argLen, char **argument, char *returnBuffer, unsigned int returnBufferLen) { \
		return NSCModuleWrapper::wrapHandleCommand(toObject.handleCommand(command, argLen, argument), returnBuffer, returnBufferLen); \
	} \
	extern int NSHasCommandHandler() { \
		return NSCModuleWrapper::wrapHasCommandHandler(toObject.hasCommandHandler()); \
	}
#define NSC_WRAPPERS_IGNORE_CMD_DEF() \
	extern int NSHandleCommand(const char* command, const unsigned int argLen, char **argument, char *returnBuffer, unsigned int returnBufferLen) { \
		return NSCAPI::failed; \
	} \
	extern int NSHasCommandHandler() { return NSCAPI::isfalse; }

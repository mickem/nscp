#include "stdafx.h"
#include <NSCHelper.h>

#define BUFF_LEN 4096


/**
 * Wrap a return string.
 * This function copies a string to a char buffer making sure the buffer has the correct length.
 *
 * @param *buffer Buffer to copy the string to.
 * @param bufLen Length of the buffer
 * @param str Th string to copy
 * @return NSCAPI::success unless the buffer is to short then it will be NSCAPI::invalidBufferLen
 */
int NSCHelper::wrapReturnString(char *buffer, unsigned int bufLen, std::string str) {
	if (str.length() > bufLen)
		return NSCAPI::invalidBufferLen;
	strncpy(buffer, str.c_str(), bufLen);
	return NSCAPI::success;
}
/**
 * Make a list out of a array of char arrays (arguments type)
 * @param argLen Length of argument array
 * @param *argument[] Argument array
 * @return Argument wrapped as a list
 */
std::list<std::string> NSCHelper::makelist(const unsigned int argLen, char *argument[]) {
	std::list<std::string> ret;
	int i=0;
	for (unsigned int i=0;i<argLen;i++) {
		std::string s = argument[i];
		ret.push_back(s);
	}
	return ret;
}
/**
 * Translate a message type into a human readable string.
 *
 * @param msgType The message type
 * @return A string representing the message type
 */
std::string NSCHelper::translateMessageType(NSCAPI::messageTypes msgType) {
	switch (msgType) {
		case NSCAPI::error:
			return "error";
		case NSCAPI::critical:
			return "critical";
		case NSCAPI::warning:
			return "warning";
		case NSCAPI::log:
			return "message";
		case NSCAPI::debug:
			return "debug";
	}
	return "unknown";
}


namespace NSCModuleHelper {
	lpNSAPIGetBasePath fNSAPIGetBasePath = NULL;
	lpNSAPIGetApplicationName fNSAPIGetApplicationName = NULL;
	lpNSAPIGetApplicationVersionStr fNSAPIGetApplicationVersionStr = NULL;
	lpNSAPIGetSettingsString fNSAPIGetSettingsString = NULL;
	lpNSAPIGetSettingsInt fNSAPIGetSettingsInt = NULL;
	lpNSAPIMessage fNSAPIMessage = NULL;
	lpNSAPIStopServer fNSAPIStopServer = NULL;
	lpNSAPIInject fNSAPIInject = NULL;
}

//////////////////////////////////////////////////////////////////////////
// Callbacks into the core
//////////////////////////////////////////////////////////////////////////

/**
 * Callback to send a message through to the core
 *
 * @param msgType Message type (debug, warning, etc.)
 * @param file File where message was generated (__FILE__)
 * @param line Line where message was generated (__LINE__)
 * @param message Message in human readable format
 */
void NSCModuleHelper::Message(int msgType, std::string file, int line, std::string message) {
	if (!fNSAPIMessage)
		throw "NSCore has not been initiated...";
	return fNSAPIMessage(msgType, file.c_str(), line, message.c_str());
}
/**
 * Inject a request command in the core (this will then be sent to the plug-in stack for processing)
 * @param command Command to inject (password should not be included.
 * @return The result (if any) of the command.
 */
std::string NSCModuleHelper::InjectCommand(std::string command) {
	if (!fNSAPIInject)
		return "NSCore has not been initiated...";
	char *buffer = new char[BUFF_LEN+1];
	if (fNSAPIInject(command.c_str(), buffer, BUFF_LEN) != NSCAPI::success)
		throw "Application name could not be retrieved";
	std::string ret = buffer;
	delete [] buffer;
	return ret;
}
/**
 * Ask the core to shutdown (only works when run as a service, o/w does nothing ?
 * @todo Check if this might cause damage if not run as a service.
 */
void NSCModuleHelper::StopService(void) {
	if (fNSAPIStopServer)
		fNSAPIStopServer();
}
/**
 * Retrieve a string from the settings subsystem (INI-file)
 * Might possibly be located in the registry in the future.
 *
 * @param section Section key (generally module specific, make sure this is "unique")
 * @param key The key to retrieve 
 * @param defaultValue A default value (if no value is set in the settings file)
 * @return the current value or defaultValue if no value is set.
 */
std::string NSCModuleHelper::getSettingsString(std::string section, std::string key, std::string defaultValue) {
	if (!fNSAPIGetSettingsString)
		throw "NSCore has not been initiated...";
	char *buffer = new char[BUFF_LEN+1];
	int x = fNSAPIGetSettingsString(section.c_str(), key.c_str(), defaultValue.c_str(), buffer, BUFF_LEN);
	std::string ret = buffer;
	delete [] buffer;
	return ret;
}
/**
 * Retrieve an int from the settings subsystem (INI-file)
 * Might possibly be located in the registry in the future.
 *
 * @param section Section key (generally module specific, make sure this is "unique")
 * @param key The key to retrieve 
 * @param defaultValue A default value (if no value is set in the settings file)
 * @return the current value or defaultValue if no value is set.
 */
int NSCModuleHelper::getSettingsInt(std::string section, std::string key, int defaultValue) {
	if (!fNSAPIGetSettingsInt)
		throw "NSCore has not been initiated...";
	return fNSAPIGetSettingsInt(section.c_str(), key.c_str(), defaultValue);
}
/**
* Retrieve the application name (in human readable format) from the core.
* @return A string representing the application name.
*/
std::string NSCModuleHelper::getApplicationName() {
	if (!fNSAPIGetApplicationName)
		return "NSCore has not been initiated...";
	char *buffer = new char[BUFF_LEN+1];
	if (fNSAPIGetApplicationName(buffer, BUFF_LEN) != NSCAPI::success)
		throw "Application name could not be retrieved";
	std::string ret = buffer;
	delete [] buffer;
	return ret;
}
/**
 * Retrieve the directory root of the application from the core.
 * @return A string representing the base path.
 */
std::string NSCModuleHelper::getBasePath() {
	if (!fNSAPIGetBasePath)
		return "NSCore has not been initiated...";
	char *buffer = new char[BUFF_LEN+1];
	if (fNSAPIGetBasePath(buffer, BUFF_LEN) != NSCAPI::success)
		throw "Base path could not be retrieved";
	std::string ret = buffer;
	delete [] buffer;
	return ret;
}
/**
 * Retrieve the application version as a string (in human readable format) from the core.
 * @return A string representing the application version.
 */
std::string NSCModuleHelper::getApplicationVersionString() {
	if (!fNSAPIGetApplicationVersionStr)
		return "NSCore has not been initiated...";
	char *buffer = new char[BUFF_LEN+1];
	int x = fNSAPIGetApplicationVersionStr(buffer, BUFF_LEN);
	std::string ret = buffer;
	delete [] buffer;
	return ret;
}

//////////////////////////////////////////////////////////////////////////
// Module helper functions
//////////////////////////////////////////////////////////////////////////
namespace NSCModuleWrapper {
	HINSTANCE hModule_ = NULL;
}
/**
 * Used to help store the module handle (and possibly other things in the future)
 * @param hModule cf. DllMain
 * @param ul_reason_for_call cf. DllMain
 * @return TRUE
 */
BOOL NSCModuleWrapper::wrapDllMain(HANDLE hModule, DWORD ul_reason_for_call)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
		hModule_ = (HINSTANCE)hModule;
		break;
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
/**
 * Retrieve the module handle from the DllMain call.
 * @return Module handle of this DLL
 */
HINSTANCE NSCModuleWrapper::getModule() {
	return hModule_;
}

/**
 * Wrapper function around the ModuleHelperInit call.
 * This wrapper retrieves all pointers and stores them for future use.
 * @param f A function pointer to a function that can be used to load function from the core.
 * @return NSCAPI::success or NSCAPI::failure
 */
int NSCModuleWrapper::wrapModuleHelperInit(NSCModuleHelper::lpNSAPILoader f) {
	NSCModuleHelper::fNSAPIGetApplicationName = (NSCModuleHelper::lpNSAPIGetApplicationName)f("NSAPIGetApplicationName");
	NSCModuleHelper::fNSAPIGetApplicationVersionStr = (NSCModuleHelper::lpNSAPIGetApplicationVersionStr)f("NSAPIGetApplicationVersionStr");
	NSCModuleHelper::fNSAPIGetSettingsInt = (NSCModuleHelper::lpNSAPIGetSettingsInt)f("NSAPIGetSettingsInt");
	NSCModuleHelper::fNSAPIGetSettingsString = (NSCModuleHelper::lpNSAPIGetSettingsString)f("NSAPIGetSettingsString");
	NSCModuleHelper::fNSAPIMessage = (NSCModuleHelper::lpNSAPIMessage)f("NSAPIMessage");
	NSCModuleHelper::fNSAPIStopServer = (NSCModuleHelper::lpNSAPIStopServer)f("NSAPIStopServer");
	NSCModuleHelper::fNSAPIInject = (NSCModuleHelper::lpNSAPIInject)f("NSAPIInject");
	NSCModuleHelper::fNSAPIGetBasePath = (NSCModuleHelper::lpNSAPIGetBasePath)f("NSAPIGetBasePath");
	return NSCAPI::success;
}
/**
* Wrap the GetModuleName function call
* @param buf Buffer to store the module name
* @param bufLen Length of buffer
* @param str String to store inside the buffer
* @return buffer copy status
*/
int NSCModuleWrapper::wrapGetModuleName(char* buf, unsigned int bufLen, std::string str) {
	return NSCHelper::wrapReturnString(buf, bufLen, str);
}
/**
 * Wrap the GetModuleVersion function call
 * @param *major Major version number
 * @param *minor Minor version number
 * @param *revision Revision
 * @param version version as a module_version
 * @return NSCAPI::success
 */
int NSCModuleWrapper::wrapGetModuleVersion(int *major, int *minor, int *revision, module_version version) {
	*major = version.major;
	*minor = version.minor;
	*revision = version.revision;
	return NSCAPI::success;
}
/**
 * Wrap the HasCommandHandler function call
 * @param has true if this module has a command handler
 * @return NSCAPI::istrue or NSCAPI::isfalse
 */
int NSCModuleWrapper::wrapHasCommandHandler(bool has) {
	if (has)
		return NSCAPI::istrue;
	return NSCAPI::isfalse;
}
/**
 * Wrap the HasMessageHandler function call
 * @param has true if this module has a message handler
 * @return NSCAPI::istrue or NSCAPI::isfalse
 */
int NSCModuleWrapper::wrapHasMessageHandler(bool has) {
	if (has)
		return NSCAPI::istrue;
	return NSCAPI::isfalse;
}
/**
 * Wrap the HandleCommand call
 * @param retStr The string to return to the core
 * @param *returnBuffer A buffer to copy the return string to
 * @param returnBufferLen length of returnBuffer
 * @return copy status or NSCAPI::isfalse if retStr is empty
 */
int NSCModuleWrapper::wrapHandleCommand(const std::string retStr, char *returnBuffer, unsigned int returnBufferLen) {
	if (retStr.empty())
		return NSCAPI::isfalse;
	return NSCHelper::wrapReturnString(returnBuffer, returnBufferLen, retStr);
}
/**
 * Wrap the NSLoadModule call
 * @param success true if module load was successfully
 * @return NSCAPI::success or NSCAPI::failed
 */
int NSCModuleWrapper::wrapLoadModule(bool success) {
	if (success)
		return NSCAPI::success;
	return NSCAPI::failed;
}
/**
 * Wrap the NSUnloadModule call
 * @param success true if module load was successfully
 * @return NSCAPI::success or NSCAPI::failed
 */
int NSCModuleWrapper::wrapUnloadModule(bool success) {
	if (success)
		return NSCAPI::success;
	return NSCAPI::failed;
}




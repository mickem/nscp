#include "stdafx.h"
#include <NSCHelper.h>
#include <assert.h>

#define BUFF_LEN 4096


#ifdef DEBUG
/**
* Wrap a return string.
* This function copies a string to a char buffer making sure the buffer has the correct length.
*
* @param *buffer Buffer to copy the string to.
* @param bufLen Length of the buffer
* @param str Th string to copy
* @return NSCAPI::success unless the buffer is to short then it will be NSCAPI::invalidBufferLen
*/
NSCAPI::nagiosReturn NSCHelper::wrapReturnString(char *buffer, unsigned int bufLen, std::string str, NSCAPI::nagiosReturn defaultReturnCode /* = NSCAPI::success */) {
	if (str.length() >= bufLen)
		return NSCAPI::returnInvalidBufferLen;
	strncpy(buffer, str.c_str(), bufLen);
	return defaultReturnCode;
}
/**
* Wrap a return string.
* This function copies a string to a char buffer making sure the buffer has the correct length.
*
* @param *buffer Buffer to copy the string to.
* @param bufLen Length of the buffer
* @param str Th string to copy
* @return NSCAPI::success unless the buffer is to short then it will be NSCAPI::invalidBufferLen
*/
NSCAPI::errorReturn NSCHelper::wrapReturnString(char *buffer, unsigned int bufLen, std::string str, NSCAPI::errorReturn defaultReturnCode /* = NSCAPI::success */) {
	if (str.length() >= bufLen)
		return NSCAPI::isInvalidBufferLen;
	strncpy(buffer, str.c_str(), bufLen);
	return defaultReturnCode;
}
#else
/**
* Wrap a return string.
* This function copies a string to a char buffer making sure the buffer has the correct length.
*
* @param *buffer Buffer to copy the string to.
* @param bufLen Length of the buffer
* @param str Th string to copy
* @param defaultReturnCode The default return code
* @return NSCAPI::success unless the buffer is to short then it will be NSCAPI::invalidBufferLen
*/
int NSCHelper::wrapReturnString(char *buffer, unsigned int bufLen, std::string str, int defaultReturnCode ) {
	// @todo deprecate this
	if (str.length() >= bufLen)
		return NSCAPI::isInvalidBufferLen;
	strncpy(buffer, str.c_str(), bufLen);
	return defaultReturnCode;
}
#endif


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
/**
 * Translate a return code into the corresponding string
 * @param returnCode 
 * @return 
 */
std::string NSCHelper::translateReturn(NSCAPI::nagiosReturn returnCode) {
	if (returnCode == NSCAPI::returnOK)
		return "OK";
	else if (returnCode == NSCAPI::returnCRIT)
		return "CRITICAL";
	else if (returnCode == NSCAPI::returnWARN)
		return "WARNING";
	else
		return "UNKNOWN";
}



namespace NSCModuleHelper {
	lpNSAPIGetBasePath fNSAPIGetBasePath = NULL;
	lpNSAPIGetApplicationName fNSAPIGetApplicationName = NULL;
	lpNSAPIGetApplicationVersionStr fNSAPIGetApplicationVersionStr = NULL;
	lpNSAPIGetSettingsSection fNSAPIGetSettingsSection = NULL;
	lpNSAPIGetSettingsString fNSAPIGetSettingsString = NULL;
	lpNSAPIGetSettingsInt fNSAPIGetSettingsInt = NULL;
	lpNSAPIMessage fNSAPIMessage = NULL;
	lpNSAPIStopServer fNSAPIStopServer = NULL;
	lpNSAPIInject fNSAPIInject = NULL;
	lpNSAPICheckLogMessages fNSAPICheckLogMessages = NULL;
	lpNSAPIEncrypt fNSAPIEncrypt = NULL;
	lpNSAPIDecrypt fNSAPIDecrypt = NULL;
	lpNSAPISetSettingsString fNSAPISetSettingsString = NULL;
	lpNSAPISetSettingsInt fNSAPISetSettingsInt = NULL;
	lpNSAPIWriteSettings fNSAPIWriteSettings = NULL;
	lpNSAPIReadSettings fNSAPIReadSettings = NULL;
	lpNSAPIRehash fNSAPIRehash = NULL;

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
 * @throws NSCMHExcpetion When core pointer set is unavailable.
 */
void NSCModuleHelper::Message(int msgType, std::string file, int line, std::string message) {
	if (fNSAPIMessage) {
		if ((msgType == NSCAPI::debug) && (!logDebug()))
			return;
		return fNSAPIMessage(msgType, file.c_str(), line, message.c_str());
	}
	else
		std::cout << "NSCore not loaded..." << std::endl << message << std::endl;
}
/**
 * Inject a request command in the core (this will then be sent to the plug-in stack for processing)
 * @param command Command to inject (password should not be included.
 * @return The result (if any) of the command.
 * @throws NSCMHExcpetion When core pointer set is unavailable or an unknown inject error occurs.
 */

/**
 * Inject a request command in the core (this will then be sent to the plug-in stack for processing)
 * @param command Command to inject
 * @param argLen The length of the argument buffer
 * @param **argument The argument buffer
 * @param *returnMessageBuffer Buffer to hold the returned message
 * @param returnMessageBufferLen Length of returnMessageBuffer
 * @param *returnPerfBuffer Buffer to hold the returned performance data
 * @param returnPerfBufferLen returnPerfBuffer
 * @return The returned status of the command
 */
NSCAPI::nagiosReturn NSCModuleHelper::InjectCommandRAW(const char* command, const unsigned int argLen, char **argument, char *returnMessageBuffer, unsigned int returnMessageBufferLen, char *returnPerfBuffer, unsigned int returnPerfBufferLen) 
{
	if (!fNSAPIInject)
		throw NSCMHExcpetion("NSCore has not been initiated...");
	return fNSAPIInject(command, argLen, argument, returnMessageBuffer, returnMessageBufferLen, returnPerfBuffer, returnPerfBufferLen);
}
/**
 * Inject a request command in the core (this will then be sent to the plug-in stack for processing)
 * @param command Command to inject (password should not be included.
 * @param argLen The length of the argument buffer
 * @param **argument The argument buffer
 * @param message The return message buffer
 * @param perf The return performance data buffer
 * @return The return of the command
 */
NSCAPI::nagiosReturn NSCModuleHelper::InjectCommand(const char* command, const unsigned int argLen, char **argument, std::string & message, std::string & perf) 
{
	if (!fNSAPIInject)
		throw NSCMHExcpetion("NSCore has not been initiated...");
	char *msgBuffer = new char[BUFF_LEN+1];
	char *perfBuffer = new char[BUFF_LEN+1];
	msgBuffer[0] = 0;
	perfBuffer[0] = 0;
	// @todo message here !
	NSCAPI::nagiosReturn retC = InjectCommandRAW(command, argLen, argument, msgBuffer, BUFF_LEN, perfBuffer, BUFF_LEN);
	switch (retC) {
		case NSCAPI::returnIgnored:
			NSC_LOG_MESSAGE_STD("No handler for command '" + command + "'.");
			break;
		case NSCAPI::returnInvalidBufferLen:
			NSC_LOG_ERROR("Inject command resulted in an invalid buffer size.");
			break;
		case NSCAPI::returnOK:
		case NSCAPI::returnCRIT:
		case NSCAPI::returnWARN:
		case NSCAPI::returnUNKNOWN:
			message = msgBuffer;
			perf = perfBuffer;
			break;
		default:
			throw NSCMHExcpetion("Unknown inject error.");
	}
	delete [] msgBuffer;
	delete [] perfBuffer;
	return retC;
}
/**
 * A wrapper around the InjetCommand that is simpler to use.
 * Parses a string by splitting and makes the array and also manages return buffers and such.
 * @param command The command to execute
 * @param buffer The buffer to split
 * @param splitChar The char to use as splitter
 * @param message The return message buffer
 * @param perf The return performance data buffer
 * @return The result of the command
 */
NSCAPI::nagiosReturn NSCModuleHelper::InjectSplitAndCommand(const char* command, char* buffer, char splitChar, std::string & message, std::string & perf)
{
	if (!fNSAPIInject)
		throw NSCMHExcpetion("NSCore has not been initiated...");
	unsigned int argLen = 0;
	char ** aBuffer;
	if (buffer)
		aBuffer= arrayBuffer::split2arrayBuffer(buffer, splitChar, argLen);
	else
		aBuffer= arrayBuffer::createEmptyArrayBuffer(argLen);
	NSCAPI::nagiosReturn ret = InjectCommand(command, argLen, aBuffer, message, perf);
	arrayBuffer::destroyArrayBuffer(aBuffer, argLen);
	return ret;
}
/**
 * A wrapper around the InjetCommand that is simpler to use.
 * @param command The command to execute
 * @param buffer The buffer to split
 * @param splitChar The char to use as splitter
 * @param message The return message buffer
 * @param perf The return performance data buffer
 * @return The result of the command
 */
NSCAPI::nagiosReturn NSCModuleHelper::InjectSplitAndCommand(const std::string command, const std::string buffer, char splitChar, std::string & message, std::string & perf)
{
	if (!fNSAPIInject)
		throw NSCMHExcpetion("NSCore has not been initiated...");
	unsigned int argLen = 0;
	char ** aBuffer;
	if (buffer.empty())
		aBuffer= arrayBuffer::createEmptyArrayBuffer(argLen);
	else
		aBuffer= arrayBuffer::split2arrayBuffer(buffer, splitChar, argLen);
	NSCAPI::nagiosReturn ret = InjectCommand(command.c_str(), argLen, aBuffer, message, perf);
	arrayBuffer::destroyArrayBuffer(aBuffer, argLen);
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
 * @throws NSCMHExcpetion When core pointer set is unavailable or an error occurs.
 */
std::string NSCModuleHelper::getSettingsString(std::string section, std::string key, std::string defaultValue) {
	if (!fNSAPIGetSettingsString)
		throw NSCMHExcpetion("NSCore has not been initiated...");
	char *buffer = new char[BUFF_LEN+1];
	if (fNSAPIGetSettingsString(section.c_str(), key.c_str(), defaultValue.c_str(), buffer, BUFF_LEN) != NSCAPI::isSuccess) {
		delete [] buffer;
		throw NSCMHExcpetion("Settings could not be retrieved.");
	}
	std::string ret = buffer;
	delete [] buffer;
	return ret;
}
/**
 * Get a section of settings strings
 * @param section The section to retrieve
 * @return The keys in the section
 */
std::list<std::string> NSCModuleHelper::getSettingsSection(std::string section) {
	if (!fNSAPIGetSettingsSection)
		throw NSCMHExcpetion("NSCore has not been initiated...");
	char ** aBuffer = NULL;
	unsigned int argLen = 0;
	if (fNSAPIGetSettingsSection(section.c_str(), &aBuffer, &argLen) != NSCAPI::isSuccess) {
		throw NSCMHExcpetion("Settings could not be retrieved.");
	}
	std::list<std::string> ret = arrayBuffer::arrayBuffer2list(argLen, aBuffer);
	arrayBuffer::destroyArrayBuffer(aBuffer, argLen);
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
 * @throws NSCMHExcpetion When core pointer set is unavailable.
 */
int NSCModuleHelper::getSettingsInt(std::string section, std::string key, int defaultValue) {
	if (!fNSAPIGetSettingsInt)
		throw NSCMHExcpetion("NSCore has not been initiated...");
	return fNSAPIGetSettingsInt(section.c_str(), key.c_str(), defaultValue);
}
/**
 * Returns the biggest of the two states
 * STATE_UNKNOWN < STATE_OK < STATE_WARNING < STATE_CRITICAL
 * @param a 
 * @param b 
 * @return 
 */
NSCAPI::nagiosReturn NSCHelper::maxState(NSCAPI::nagiosReturn a, NSCAPI::nagiosReturn b)
{
	if (a == NSCAPI::returnCRIT || b == NSCAPI::returnCRIT)
		return NSCAPI::returnCRIT;
	else if (a == NSCAPI::returnWARN || b == NSCAPI::returnWARN)
		return NSCAPI::returnWARN;
	else if (a == NSCAPI::returnOK || b == NSCAPI::returnOK)
		return NSCAPI::returnOK;
	else if (a == NSCAPI::returnUNKNOWN || b == NSCAPI::returnUNKNOWN)
		return NSCAPI::returnUNKNOWN;
	return NSCAPI::returnUNKNOWN;
}
/**
 * Retrieve the application name (in human readable format) from the core.
 * @return A string representing the application name.
 * @throws NSCMHExcpetion When core pointer set is unavailable or an unexpected error occurs.
 */
std::string NSCModuleHelper::getApplicationName() {
	if (!fNSAPIGetApplicationName)
		throw NSCMHExcpetion("NSCore has not been initiated...");
	char *buffer = new char[BUFF_LEN+1];
	if (fNSAPIGetApplicationName(buffer, BUFF_LEN) != NSCAPI::isSuccess) {
		delete [] buffer;
		throw NSCMHExcpetion("Application name could not be retrieved");
	}
	std::string ret = buffer;
	delete [] buffer;
	return ret;
}
/**
 * Retrieve the directory root of the application from the core.
 * @return A string representing the base path.
 * @throws NSCMHExcpetion When core pointer set is unavailable or an unexpected error occurs.
 */
std::string NSCModuleHelper::getBasePath() {
	if (!fNSAPIGetBasePath)
		throw NSCMHExcpetion("NSCore has not been initiated...");
	char *buffer = new char[BUFF_LEN+1];
	if (fNSAPIGetBasePath(buffer, BUFF_LEN) != NSCAPI::isSuccess) {
		delete [] buffer;
		throw NSCMHExcpetion("Base path could not be retrieved");
	}
	std::string ret = buffer;
	delete [] buffer;
	return ret;
}


bool NSCModuleHelper::logDebug() {
	typedef enum status {unknown, debug, nodebug };
	static status d = unknown;
	if (d == unknown) {
		if (checkLogMessages(debug)== NSCAPI::istrue)
			d = debug;
		else
			d = nodebug;
	}
	return (d == debug);
}

std::string NSCModuleHelper::Encrypt(std::string str, unsigned int algorithm) {
	if (!fNSAPIEncrypt)
		throw NSCMHExcpetion("NSCore has not been initiated...");
	unsigned int len = 0;
	// @todo investigate potential problems with static_cast<unsigned int>
	fNSAPIEncrypt(algorithm, str.c_str(), static_cast<unsigned int>(str.size()), NULL, &len);
	len+=2;
	char *buf = new char[len+1];
	NSCAPI::errorReturn ret = fNSAPIEncrypt(algorithm, str.c_str(), static_cast<unsigned int>(str.size()), buf, &len);
	if (ret == NSCAPI::isSuccess) {
		std::string ret = buf;
		delete [] buf;
		return ret;
	}
	return "";
}
std::string NSCModuleHelper::Decrypt(std::string str, unsigned int algorithm) {
	if (!fNSAPIDecrypt)
		throw NSCMHExcpetion("NSCore has not been initiated...");
	unsigned int len = 0;
	// @todo investigate potential problems with: static_cast<unsigned int>(str.size())
	fNSAPIDecrypt(algorithm, str.c_str(), static_cast<unsigned int>(str.size()), NULL, &len);
	len+=2;
	char *buf = new char[len+1];
	NSCAPI::errorReturn ret = fNSAPIDecrypt(algorithm, str.c_str(), static_cast<unsigned int>(str.size()), buf, &len);
	if (ret == NSCAPI::isSuccess) {
		std::string ret = buf;
		delete [] buf;
		return ret;
	}
	return "";
}
NSCAPI::errorReturn NSCModuleHelper::SetSettingsString(std::string section, std::string key, std::string value) {
	if (!fNSAPISetSettingsString)
		throw NSCMHExcpetion("NSCore has not been initiated...");
	return fNSAPISetSettingsString(section.c_str(), key.c_str(), value.c_str());
}
NSCAPI::errorReturn NSCModuleHelper::SetSettingsInt(std::string section, std::string key, int value) {
	if (!fNSAPISetSettingsInt)
		throw NSCMHExcpetion("NSCore has not been initiated...");
	return fNSAPISetSettingsInt(section.c_str(), key.c_str(), value);
}
NSCAPI::errorReturn NSCModuleHelper::WriteSettings(int type) {
	if (!fNSAPIWriteSettings)
		throw NSCMHExcpetion("NSCore has not been initiated...");
	return fNSAPIWriteSettings(type);
}
NSCAPI::errorReturn NSCModuleHelper::ReadSettings(int type) {
	if (!fNSAPIReadSettings)
		throw NSCMHExcpetion("NSCore has not been initiated...");
	return fNSAPIReadSettings(type);
}
NSCAPI::errorReturn NSCModuleHelper::Rehash(int flag) {
	if (!fNSAPIRehash)
		throw NSCMHExcpetion("NSCore has not been initiated...");
	return fNSAPIRehash(flag);
}


bool NSCModuleHelper::checkLogMessages(int type) {
	if (!fNSAPICheckLogMessages)
		throw NSCMHExcpetion("NSCore has not been initiated...");
	return fNSAPICheckLogMessages(type) == NSCAPI::istrue;
}
/**
 * Retrieve the application version as a string (in human readable format) from the core.
 * @return A string representing the application version.
 * @throws NSCMHExcpetion When core pointer set is unavailable.
 */
std::string NSCModuleHelper::getApplicationVersionString() {
	if (!fNSAPIGetApplicationVersionStr)
		throw NSCMHExcpetion("NSCore has not been initiated...");
	char *buffer = new char[BUFF_LEN+1];
	int x = fNSAPIGetApplicationVersionStr(buffer, BUFF_LEN);
	std::string ret = buffer;
	delete [] buffer;
	return ret;
}

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
	NSCModuleHelper::fNSAPIGetSettingsSection = (NSCModuleHelper::lpNSAPIGetSettingsSection)f("NSAPIGetSettingsSection");
	NSCModuleHelper::fNSAPIMessage = (NSCModuleHelper::lpNSAPIMessage)f("NSAPIMessage");
	NSCModuleHelper::fNSAPIStopServer = (NSCModuleHelper::lpNSAPIStopServer)f("NSAPIStopServer");
	NSCModuleHelper::fNSAPIInject = (NSCModuleHelper::lpNSAPIInject)f("NSAPIInject");
	NSCModuleHelper::fNSAPIGetBasePath = (NSCModuleHelper::lpNSAPIGetBasePath)f("NSAPIGetBasePath");
	NSCModuleHelper::fNSAPICheckLogMessages = (NSCModuleHelper::lpNSAPICheckLogMessages)f("NSAPICheckLogMessages");
	NSCModuleHelper::fNSAPIDecrypt = (NSCModuleHelper::lpNSAPIDecrypt)f("NSAPIDecrypt");
	NSCModuleHelper::fNSAPIEncrypt = (NSCModuleHelper::lpNSAPIEncrypt)f("NSAPIEncrypt");
	NSCModuleHelper::fNSAPISetSettingsString = (NSCModuleHelper::lpNSAPISetSettingsString)f("NSAPISetSettingsString");
	NSCModuleHelper::fNSAPISetSettingsInt = (NSCModuleHelper::lpNSAPISetSettingsInt)f("NSAPISetSettingsInt");
	NSCModuleHelper::fNSAPIWriteSettings = (NSCModuleHelper::lpNSAPIWriteSettings)f("NSAPIWriteSettings");
	NSCModuleHelper::fNSAPIReadSettings = (NSCModuleHelper::lpNSAPIReadSettings)f("NSAPIReadSettings");
	NSCModuleHelper::fNSAPIRehash = (NSCModuleHelper::lpNSAPIRehash)f("NSAPIRehash");
	return NSCAPI::isSuccess;
}
/**
* Wrap the GetModuleName function call
* @param buf Buffer to store the module name
* @param bufLen Length of buffer
* @param str String to store inside the buffer
* @return buffer copy status
*/
NSCAPI::errorReturn NSCModuleWrapper::wrapGetModuleName(char* buf, unsigned int bufLen, std::string str) {
	return NSCHelper::wrapReturnString(buf, bufLen, str, NSCAPI::isSuccess);
}

NSCAPI::errorReturn NSCModuleWrapper::wrapGetConfigurationMeta(char* buf, unsigned int bufLen, std::string str) {
	return NSCHelper::wrapReturnString(buf, bufLen, str, NSCAPI::isSuccess);
}
/**
 * Wrap the GetModuleVersion function call
 * @param *major Major version number
 * @param *minor Minor version number
 * @param *revision Revision
 * @param version version as a module_version
 * @return NSCAPI::success
 */
NSCAPI::errorReturn NSCModuleWrapper::wrapGetModuleVersion(int *major, int *minor, int *revision, module_version version) {
	*major = version.major;
	*minor = version.minor;
	*revision = version.revision;
	return NSCAPI::isSuccess;
}
/**
 * Wrap the HasCommandHandler function call
 * @param has true if this module has a command handler
 * @return NSCAPI::istrue or NSCAPI::isfalse
 */
NSCAPI::boolReturn NSCModuleWrapper::wrapHasCommandHandler(bool has) {
	if (has)
		return NSCAPI::istrue;
	return NSCAPI::isfalse;
}
/**
 * Wrap the HasMessageHandler function call
 * @param has true if this module has a message handler
 * @return NSCAPI::istrue or NSCAPI::isfalse
 */
NSCAPI::boolReturn NSCModuleWrapper::wrapHasMessageHandler(bool has) {
	if (has)
		return NSCAPI::istrue;
	return NSCAPI::isfalse;
}
/**
 * Wrap the HandleCommand call
 * @param retResult The returned result
 * @param retMessage The returned message
 * @param retPerformance The returned performance data
 * @param *returnBufferMessage The return message buffer
 * @param returnBufferMessageLen The return message buffer length
 * @param *returnBufferPerf The return perfomance data buffer
 * @param returnBufferPerfLen The return perfomance data buffer length
 * @return the return code
 */
NSCAPI::nagiosReturn NSCModuleWrapper::wrapHandleCommand(NSCAPI::nagiosReturn retResult, const std::string retMessage, const std::string retPerformance, char *returnBufferMessage, unsigned int returnBufferMessageLen, char *returnBufferPerf, unsigned int returnBufferPerfLen) {
	if (retMessage.empty())
		return NSCAPI::returnIgnored;
	NSCAPI::nagiosReturn ret = NSCHelper::wrapReturnString(returnBufferMessage, returnBufferMessageLen, retMessage, retResult);
	return NSCHelper::wrapReturnString(returnBufferPerf, returnBufferPerfLen, retPerformance, ret);
}
/**
 * Wrap the NSLoadModule call
 * @param success true if module load was successfully
 * @return NSCAPI::success or NSCAPI::failed
 */
int NSCModuleWrapper::wrapLoadModule(bool success) {
	if (success)
		return NSCAPI::isSuccess;
	return NSCAPI::hasFailed;
}
/**
 * Wrap the NSUnloadModule call
 * @param success true if module load was successfully
 * @return NSCAPI::success or NSCAPI::failed
 */
int NSCModuleWrapper::wrapUnloadModule(bool success) {
	if (success)
		return NSCAPI::isSuccess;
	return NSCAPI::hasFailed;
}




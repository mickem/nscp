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

#include <NSCHelper.h>
#include <assert.h>
#include <msvc_wrappers.h>
#include <config.h>
#include <strEx.h>

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
NSCAPI::nagiosReturn NSCHelper::wrapReturnString(char *buffer, unsigned int bufLen, std::wstring str, NSCAPI::nagiosReturn defaultReturnCode /* = NSCAPI::success */) {
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
NSCAPI::errorReturn NSCHelper::wrapReturnString(char *buffer, unsigned int bufLen, std::wstring str, NSCAPI::errorReturn defaultReturnCode /* = NSCAPI::success */) {
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
int NSCHelper::wrapReturnString(TCHAR *buffer, unsigned int bufLen, std::wstring str, int defaultReturnCode ) {
	// @todo deprecate this
	if (str.length() >= bufLen) {
		std::wstring sstr = str.substr(0, min(10, str.length()));
		NSC_DEBUG_MSG_STD(_T("String (") + strEx::itos(str.length()) + _T(") to long to fit inside buffer(") + strEx::itos(bufLen) + _T(") : ") + sstr);
		return NSCAPI::isInvalidBufferLen;
	}
	wcsncpy_s(buffer, bufLen, str.c_str(), bufLen);
	return defaultReturnCode;
}
#endif


/**
 * Translate a message type into a human readable string.
 *
 * @param msgType The message type
 * @return A string representing the message type
 */
std::wstring NSCHelper::translateMessageType(NSCAPI::messageTypes msgType) {
	switch (msgType) {
		case NSCAPI::error:
			return _T("error");
		case NSCAPI::critical:
			return _T("critical");
		case NSCAPI::warning:
			return _T("warning");
		case NSCAPI::log:
			return _T("message");
		case NSCAPI::debug:
			return _T("debug");
	}
	return _T("unknown");
}
/**
 * Translate a return code into the corresponding string
 * @param returnCode 
 * @return 
 */
std::wstring NSCHelper::translateReturn(NSCAPI::nagiosReturn returnCode) {
	if (returnCode == NSCAPI::returnOK)
		return _T("OK");
	else if (returnCode == NSCAPI::returnCRIT)
		return _T("CRITICAL");
	else if (returnCode == NSCAPI::returnWARN)
		return _T("WARNING");
	else if (returnCode == NSCAPI::returnUNKNOWN)
		return _T("WARNING");
	else
		return _T("BAD_CODE");
}
/**
* Translate a string into the corresponding return code 
* @param returnCode 
* @return 
*/
NSCAPI::nagiosReturn NSCHelper::translateReturn(std::wstring str) {
	if (str == _T("OK"))
		return NSCAPI::returnOK;
	else if (str == _T("CRITICAL"))
		return NSCAPI::returnCRIT;
	else if (str == _T("WARNING"))
		return NSCAPI::returnWARN;
	else 
		return NSCAPI::returnUNKNOWN;
}



namespace NSCModuleHelper {
	lpNSAPIGetBasePath fNSAPIGetBasePath = NULL;
	lpNSAPIGetApplicationName fNSAPIGetApplicationName = NULL;
	lpNSAPIGetApplicationVersionStr fNSAPIGetApplicationVersionStr = NULL;
	lpNSAPIGetSettingsSection fNSAPIGetSettingsSection = NULL;
	lpNSAPIReleaseSettingsSectionBuffer fNSAPIReleaseSettingsSectionBuffer = NULL;
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
	lpNSAPIDescribeCommand fNSAPIDescribeCommand= NULL;
	lpNSAPIGetAllCommandNames fNSAPIGetAllCommandNames= NULL;
	lpNSAPIReleaseAllCommandNamessBuffer fNSAPIReleaseAllCommandNamessBuffer= NULL;
	lpNSAPIRegisterCommand fNSAPIRegisterCommand= NULL;
	unsigned int buffer_length;

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
void NSCModuleHelper::Message(int msgType, std::wstring file, int line, std::wstring message) {
	if (fNSAPIMessage) {
		if ((msgType == NSCAPI::debug) && (!logDebug()))
			return;
		/*
		std::wstring::size_type pos = file.find_last_of("\\");
		if (pos != std::wstring::npos)
			file = file.substr(pos);
			*/
		return fNSAPIMessage(msgType, file.c_str(), line, message.c_str());
	}
	else
		std::wcout << _T("NSCore not loaded...") << std::endl << message << std::endl;
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
NSCAPI::nagiosReturn NSCModuleHelper::InjectCommandRAW(const TCHAR* command, const unsigned int argLen, TCHAR **argument, TCHAR *returnMessageBuffer, unsigned int returnMessageBufferLen, TCHAR *returnPerfBuffer, unsigned int returnPerfBufferLen) 
{
	if (!fNSAPIInject)
		throw NSCMHExcpetion(_T("NSCore has not been initiated..."));
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
NSCAPI::nagiosReturn NSCModuleHelper::InjectCommand(const TCHAR* command, const unsigned int argLen, TCHAR **argument, std::wstring & message, std::wstring & perf) 
{
	if (!fNSAPIInject)
		throw NSCMHExcpetion(_T("NSCore has not been initiated..."));
	unsigned int buf_len = getBufferLength();
	TCHAR *msgBuffer = new TCHAR[buf_len+1];
	TCHAR *perfBuffer = new TCHAR[buf_len+1];
	msgBuffer[0] = 0;
	perfBuffer[0] = 0;
	NSCAPI::nagiosReturn retC = InjectCommandRAW(command, argLen, argument, msgBuffer, buf_len, perfBuffer, buf_len);
	switch (retC) {
		case NSCAPI::returnIgnored:
			NSC_LOG_MESSAGE_STD(_T("No handler for command '") + command + _T("'."));
			break;
		case NSCAPI::returnInvalidBufferLen:
			NSC_LOG_ERROR(_T("Inject buffer to small, increase the value of: string_length."));
			break;
		case NSCAPI::returnOK:
		case NSCAPI::returnCRIT:
		case NSCAPI::returnWARN:
		case NSCAPI::returnUNKNOWN:
			message = msgBuffer;
			perf = perfBuffer;
			break;
		default:
			throw NSCMHExcpetion(_T("Unknown inject error."));
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
NSCAPI::nagiosReturn NSCModuleHelper::InjectSplitAndCommand(const TCHAR* command, TCHAR* buffer, TCHAR splitChar, std::wstring & message, std::wstring & perf)
{
	if (!fNSAPIInject)
		throw NSCMHExcpetion(_T("NSCore has not been initiated..."));
	unsigned int argLen = 0;
	TCHAR ** aBuffer;
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
NSCAPI::nagiosReturn NSCModuleHelper::InjectSplitAndCommand(const std::wstring command, const std::wstring buffer, TCHAR splitChar, std::wstring & message, std::wstring & perf, bool escape)
{
	if (!fNSAPIInject)
		throw NSCMHExcpetion(_T("NSCore has not been initiated..."));
	unsigned int argLen = 0;
	TCHAR ** aBuffer;
	if (buffer.empty())
		aBuffer= arrayBuffer::createEmptyArrayBuffer(argLen);
	else
		aBuffer= arrayBuffer::split2arrayBuffer(buffer, splitChar, argLen, escape);
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
std::wstring NSCModuleHelper::getSettingsString(std::wstring section, std::wstring key, std::wstring defaultValue) {
	if (!fNSAPIGetSettingsString)
		throw NSCMHExcpetion(_T("NSCore has not been initiated..."));
	unsigned int buf_len = getBufferLength();
	TCHAR *buffer = new TCHAR[buf_len+1];
	if (fNSAPIGetSettingsString(section.c_str(), key.c_str(), defaultValue.c_str(), buffer, buf_len) != NSCAPI::isSuccess) {
		delete [] buffer;
		throw NSCMHExcpetion(_T("Settings could not be retrieved."));
	}
	std::wstring ret = buffer;
	delete [] buffer;
	return ret;
}
/**
 * Get a section of settings strings
 * @param section The section to retrieve
 * @return The keys in the section
 */
std::list<std::wstring> NSCModuleHelper::getSettingsSection(std::wstring section) {
	if (!fNSAPIGetSettingsSection)
		throw NSCMHExcpetion(_T("NSCore has not been initiated..."));
	arrayBuffer::arrayBuffer aBuffer = NULL;
	unsigned int argLen = 0;
	if (fNSAPIGetSettingsSection(section.c_str(), &aBuffer, &argLen) != NSCAPI::isSuccess) {
		throw NSCMHExcpetion(_T("Settings could not be retrieved."));
	}
	std::list<std::wstring> ret = arrayBuffer::arrayBuffer2list(argLen, aBuffer);
	if (fNSAPIReleaseSettingsSectionBuffer(&aBuffer, &argLen) != NSCAPI::isSuccess) {
		throw NSCMHExcpetion(_T("Settings could not be destroyed."));
	}
	assert(aBuffer == NULL);
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
int NSCModuleHelper::getSettingsInt(std::wstring section, std::wstring key, int defaultValue) {
	if (!fNSAPIGetSettingsInt)
		throw NSCMHExcpetion(_T("NSCore has not been initiated..."));
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
std::wstring NSCModuleHelper::getApplicationName() {
	if (!fNSAPIGetApplicationName)
		throw NSCMHExcpetion(_T("NSCore has not been initiated..."));
	unsigned int buf_len = getBufferLength();
	TCHAR *buffer = new TCHAR[buf_len+1];
	if (fNSAPIGetApplicationName(buffer, buf_len) != NSCAPI::isSuccess) {
		delete [] buffer;
		throw NSCMHExcpetion(_T("Application name could not be retrieved"));
	}
	std::wstring ret = buffer;
	delete [] buffer;
	return ret;
}
/**
 * Retrieve the directory root of the application from the core.
 * @return A string representing the base path.
 * @throws NSCMHExcpetion When core pointer set is unavailable or an unexpected error occurs.
 */
std::wstring NSCModuleHelper::getBasePath() {
	if (!fNSAPIGetBasePath)
		throw NSCMHExcpetion(_T("NSCore has not been initiated..."));
	unsigned int buf_len = getBufferLength();
	TCHAR *buffer = new TCHAR[buf_len+1];
	if (fNSAPIGetBasePath(buffer, buf_len) != NSCAPI::isSuccess) {
		delete [] buffer;
		throw NSCMHExcpetion(_T("Base path could not be retrieved"));
	}
	std::wstring ret = buffer;
	delete [] buffer;
	return ret;
}

unsigned int NSCModuleHelper::getBufferLength() {
	static unsigned int len = 0;
	if (len == 0) {
		len = getSettingsInt(MAIN_SECTION_TITLE, MAIN_STRING_LENGTH, MAIN_STRING_LENGTH_DEFAULT);
	}
	return len;
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

std::wstring NSCModuleHelper::Encrypt(std::wstring str, unsigned int algorithm) {
	if (!fNSAPIEncrypt)
		throw NSCMHExcpetion(_T("NSCore has not been initiated..."));
	unsigned int len = 0;
	// @todo investigate potential problems with static_cast<unsigned int>
	fNSAPIEncrypt(algorithm, str.c_str(), static_cast<unsigned int>(str.size()), NULL, &len);
	len+=2;
	TCHAR *buf = new TCHAR[len+1];
	NSCAPI::errorReturn ret = fNSAPIEncrypt(algorithm, str.c_str(), static_cast<unsigned int>(str.size()), buf, &len);
	if (ret == NSCAPI::isSuccess) {
		std::wstring ret = buf;
		delete [] buf;
		return ret;
	}
	return _T("");
}
std::wstring NSCModuleHelper::Decrypt(std::wstring str, unsigned int algorithm) {
	if (!fNSAPIDecrypt)
		throw NSCMHExcpetion(_T("NSCore has not been initiated..."));
	unsigned int len = 0;
	// @todo investigate potential problems with: static_cast<unsigned int>(str.size())
	fNSAPIDecrypt(algorithm, str.c_str(), static_cast<unsigned int>(str.size()), NULL, &len);
	len+=2;
	TCHAR *buf = new TCHAR[len+1];
	NSCAPI::errorReturn ret = fNSAPIDecrypt(algorithm, str.c_str(), static_cast<unsigned int>(str.size()), buf, &len);
	if (ret == NSCAPI::isSuccess) {
		std::wstring ret = buf;
		delete [] buf;
		return ret;
	}
	return _T("");
}
NSCAPI::errorReturn NSCModuleHelper::SetSettingsString(std::wstring section, std::wstring key, std::wstring value) {
	if (!fNSAPISetSettingsString)
		throw NSCMHExcpetion(_T("NSCore has not been initiated..."));
	return fNSAPISetSettingsString(section.c_str(), key.c_str(), value.c_str());
}
NSCAPI::errorReturn NSCModuleHelper::SetSettingsInt(std::wstring section, std::wstring key, int value) {
	if (!fNSAPISetSettingsInt)
		throw NSCMHExcpetion(_T("NSCore has not been initiated..."));
	return fNSAPISetSettingsInt(section.c_str(), key.c_str(), value);
}
NSCAPI::errorReturn NSCModuleHelper::WriteSettings(int type) {
	if (!fNSAPIWriteSettings)
		throw NSCMHExcpetion(_T("NSCore has not been initiated..."));
	return fNSAPIWriteSettings(type);
}
NSCAPI::errorReturn NSCModuleHelper::ReadSettings(int type) {
	if (!fNSAPIReadSettings)
		throw NSCMHExcpetion(_T("NSCore has not been initiated..."));
	return fNSAPIReadSettings(type);
}
NSCAPI::errorReturn NSCModuleHelper::Rehash(int flag) {
	if (!fNSAPIRehash)
		throw NSCMHExcpetion(_T("NSCore has not been initiated..."));
	return fNSAPIRehash(flag);
}

std::list<std::wstring> NSCModuleHelper::getAllCommandNames() {
	if (!fNSAPIGetAllCommandNames || !fNSAPIReleaseAllCommandNamessBuffer )
		throw NSCMHExcpetion(_T("NSCore has not been initiated..."));
	arrayBuffer::arrayBuffer aBuffer = NULL;
	unsigned int argLen = 0;
	if (fNSAPIGetAllCommandNames(&aBuffer, &argLen) != NSCAPI::isSuccess) {
		throw NSCMHExcpetion(_T("Commands could not be retrieved."));
	}
	std::list<std::wstring> ret = arrayBuffer::arrayBuffer2list(argLen, aBuffer);
	if (fNSAPIReleaseAllCommandNamessBuffer(&aBuffer, &argLen) != NSCAPI::isSuccess) {
		throw NSCMHExcpetion(_T("Commands could not be destroyed."));
	}
	assert(aBuffer == NULL);
	return ret;
}
std::wstring NSCModuleHelper::describeCommand(std::wstring command) {
	if (!fNSAPIDescribeCommand)
		throw NSCMHExcpetion(_T("NSCore has not been initiated..."));
	unsigned int buf_len = getBufferLength();
	TCHAR *buffer = new TCHAR[buf_len+1];
	if (fNSAPIDescribeCommand(command.c_str(), buffer, buf_len) != NSCAPI::isSuccess) {
		delete [] buffer;
		throw NSCMHExcpetion(_T("Base path could not be retrieved"));
	}
	std::wstring ret = buffer;
	delete [] buffer;
	return ret;
}
void NSCModuleHelper::registerCommand(std::wstring command, std::wstring description) {
	if (!fNSAPIRegisterCommand)
		throw NSCMHExcpetion(_T("NSCore has not been initiated..."));
	fNSAPIRegisterCommand(command.c_str(), description.c_str());
}


bool NSCModuleHelper::checkLogMessages(int type) {
	if (!fNSAPICheckLogMessages)
		throw NSCMHExcpetion(_T("NSCore has not been initiated..."));
	return fNSAPICheckLogMessages(type) == NSCAPI::istrue;
}
/**
 * Retrieve the application version as a string (in human readable format) from the core.
 * @return A string representing the application version.
 * @throws NSCMHExcpetion When core pointer set is unavailable.
 */
std::wstring NSCModuleHelper::getApplicationVersionString() {
	if (!fNSAPIGetApplicationVersionStr)
		throw NSCMHExcpetion(_T("NSCore has not been initiated..."));
	unsigned int buf_len = getBufferLength();
	TCHAR *buffer = new TCHAR[buf_len+1];
	if (fNSAPIGetApplicationVersionStr(buffer, buf_len) != NSCAPI::isSuccess) {
		delete [] buffer;
		return _T("");
	}
	std::wstring ret = buffer;
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
	NSCModuleHelper::fNSAPIGetApplicationName = (NSCModuleHelper::lpNSAPIGetApplicationName)f(_T("NSAPIGetApplicationName"));
	NSCModuleHelper::fNSAPIGetApplicationVersionStr = (NSCModuleHelper::lpNSAPIGetApplicationVersionStr)f(_T("NSAPIGetApplicationVersionStr"));
	NSCModuleHelper::fNSAPIGetSettingsInt = (NSCModuleHelper::lpNSAPIGetSettingsInt)f(_T("NSAPIGetSettingsInt"));
	NSCModuleHelper::fNSAPIGetSettingsString = (NSCModuleHelper::lpNSAPIGetSettingsString)f(_T("NSAPIGetSettingsString"));
	NSCModuleHelper::fNSAPIGetSettingsSection = (NSCModuleHelper::lpNSAPIGetSettingsSection)f(_T("NSAPIGetSettingsSection"));
	NSCModuleHelper::fNSAPIReleaseSettingsSectionBuffer = (NSCModuleHelper::lpNSAPIReleaseSettingsSectionBuffer)f(_T("NSAPIReleaseSettingsSectionBuffer"));
	NSCModuleHelper::fNSAPIMessage = (NSCModuleHelper::lpNSAPIMessage)f(_T("NSAPIMessage"));
	NSCModuleHelper::fNSAPIStopServer = (NSCModuleHelper::lpNSAPIStopServer)f(_T("NSAPIStopServer"));
	NSCModuleHelper::fNSAPIInject = (NSCModuleHelper::lpNSAPIInject)f(_T("NSAPIInject"));
	NSCModuleHelper::fNSAPIGetBasePath = (NSCModuleHelper::lpNSAPIGetBasePath)f(_T("NSAPIGetBasePath"));
	NSCModuleHelper::fNSAPICheckLogMessages = (NSCModuleHelper::lpNSAPICheckLogMessages)f(_T("NSAPICheckLogMessages"));
	NSCModuleHelper::fNSAPIDecrypt = (NSCModuleHelper::lpNSAPIDecrypt)f(_T("NSAPIDecrypt"));
	NSCModuleHelper::fNSAPIEncrypt = (NSCModuleHelper::lpNSAPIEncrypt)f(_T("NSAPIEncrypt"));
	NSCModuleHelper::fNSAPISetSettingsString = (NSCModuleHelper::lpNSAPISetSettingsString)f(_T("NSAPISetSettingsString"));
	NSCModuleHelper::fNSAPISetSettingsInt = (NSCModuleHelper::lpNSAPISetSettingsInt)f(_T("NSAPISetSettingsInt"));
	NSCModuleHelper::fNSAPIWriteSettings = (NSCModuleHelper::lpNSAPIWriteSettings)f(_T("NSAPIWriteSettings"));
	NSCModuleHelper::fNSAPIReadSettings = (NSCModuleHelper::lpNSAPIReadSettings)f(_T("NSAPIReadSettings"));
	NSCModuleHelper::fNSAPIRehash = (NSCModuleHelper::lpNSAPIRehash)f(_T("NSAPIRehash"));

	NSCModuleHelper::fNSAPIDescribeCommand = (NSCModuleHelper::lpNSAPIDescribeCommand)f(_T("NSAPIDescribeCommand"));
	NSCModuleHelper::fNSAPIGetAllCommandNames = (NSCModuleHelper::lpNSAPIGetAllCommandNames)f(_T("NSAPIGetAllCommandNames"));
	NSCModuleHelper::fNSAPIReleaseAllCommandNamessBuffer = (NSCModuleHelper::lpNSAPIReleaseAllCommandNamessBuffer)f(_T("NSAPIReleaseAllCommandNamessBuffer"));
	NSCModuleHelper::fNSAPIRegisterCommand = (NSCModuleHelper::lpNSAPIRegisterCommand)f(_T("NSAPIRegisterCommand"));

	return NSCAPI::isSuccess;
}
/**
* Wrap the GetModuleName function call
* @param buf Buffer to store the module name
* @param bufLen Length of buffer
* @param str String to store inside the buffer
* @	 copy status
*/
NSCAPI::errorReturn NSCModuleWrapper::wrapGetModuleName(TCHAR* buf, unsigned int bufLen, std::wstring str) {
	return NSCHelper::wrapReturnString(buf, bufLen, str, NSCAPI::isSuccess);
}

NSCAPI::errorReturn NSCModuleWrapper::wrapGetConfigurationMeta(TCHAR* buf, unsigned int bufLen, std::wstring str) {
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
NSCAPI::nagiosReturn NSCModuleWrapper::wrapHandleCommand(NSCAPI::nagiosReturn retResult, const std::wstring retMessage, const std::wstring retPerformance, TCHAR *returnBufferMessage, unsigned int returnBufferMessageLen, TCHAR *returnBufferPerf, unsigned int returnBufferPerfLen) {
	if (retMessage.empty())
		return NSCAPI::returnIgnored;
	NSCAPI::nagiosReturn ret = NSCHelper::wrapReturnString(returnBufferMessage, returnBufferMessageLen, retMessage, retResult);
	if (!NSCHelper::isMyNagiosReturn(ret)) {
		NSC_LOG_ERROR(_T("A module returned an invalid return code"));
	}
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




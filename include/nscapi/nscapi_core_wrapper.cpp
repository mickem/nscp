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

#include <boost/foreach.hpp>
#include <boost/tokenizer.hpp>

#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/nscapi_plugin_wrapper.hpp>
#include <settings/macros.h>
#include <arrayBuffer.h>
#include <strEx.h>

#include "../libs/protobuf/plugin.proto.h"

using namespace nscp::helpers;

#define CORE_LOG_ERROR_STD(msg) CORE_LOG_ERROR(((std::wstring)msg).c_str())
#define CORE_LOG_ERROR(msg) CORE_ANY_MSG(msg,NSCAPI::error)

#define CORE_LOG_CRITICAL_STD(msg) CORE_LOG_CRITICAL(((std::wstring)msg).c_str())
#define CORE_LOG_CRITICAL(msg) CORE_ANY_MSG(msg,NSCAPI::critical)

#define CORE_LOG_MESSAGE_STD(msg) CORE_LOG_MESSAGE(((std::wstring)msg).c_str())
#define CORE_LOG_MESSAGE(msg) CORE_ANY_MSG(msg,NSCAPI::log)

#define CORE_DEBUG_MSG_STD(msg) CORE_DEBUG_MSG((std::wstring)msg)
#define CORE_DEBUG_MSG(msg) CORE_ANY_MSG(msg,NSCAPI::debug)

#define CORE_ANY_MSG(msg, type) Message(type, __FILE__, __LINE__, msg)


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
 * @throws nscapi::nscapi_exception When core pointer set is unavailable.
 */
void nscapi::core_wrapper::Message(int msgType, std::string file, int line, std::wstring logMessage) {
	if (fNSAPIMessage) {
		if ((msgType == NSCAPI::debug) && (!logDebug()))
			return;
		LogMessage::Message_Level msgType = ::LogMessage::Message_Level::Message_Level_LOG_WARNING;
		if (msgType == NSCAPI::warning)
			msgType = ::LogMessage::Message_Level::Message_Level_LOG_WARNING;
		std::string str;
		try {
			LogMessage::LogMessage message;
			LogMessage::Message *msg = message.add_message();
			msg->set_level(msgType);
			msg->set_file(file);
			msg->set_line(line);
			msg->set_message(to_string(logMessage));
			if (!message.SerializeToString(&str)) {
				std::cout << "Failed to generate message";
			}
			return fNSAPIMessage(str.c_str(), str.size());
		} catch (...) {
			std::wcout << _T("Failed to generate message: ");
		}
// 		return fNSAPIMessage(to_string(logMessage).c_str(), logMessage.size());
	}
	else
		std::wcout << _T("*** *** *** NSCore not loaded, dumping log: ") << to_wstring(file) << _T(":") << line << _T(": ") << std::endl << logMessage << std::endl;
}
/**
 * Inject a request command in the core (this will then be sent to the plug-in stack for processing)
 * @param command Command to inject (password should not be included.
 * @return The result (if any) of the command.
 * @throws nscapi::nscapi_exception When core pointer set is unavailable or an unknown inject error occurs.
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
NSCAPI::nagiosReturn nscapi::core_wrapper::InjectCommandRAW(const wchar_t* command, const char *request, const unsigned int request_len, char **response, unsigned int *response_len) 
{
	if (!fNSAPIInject)
		throw nscapi::nscapi_exception(_T("NSCore has not been initiated..."));
	return fNSAPIInject(command, request, request_len, response, response_len);
}

void nscapi::core_wrapper::DestroyBuffer(char**buffer) {
	if (!fNSAPIDestroyBuffer)
		throw nscapi::nscapi_exception(_T("NSCore has not been initiated..."));
	return fNSAPIDestroyBuffer(buffer);
}

NSCAPI::errorReturn nscapi::core_wrapper::NotifyChannel(std::wstring channel, std::wstring command, NSCAPI::nagiosReturn code, std::string result) {
	if (!fNSAPINotify)
		throw nscapi::nscapi_exception(_T("NSCore has not been initiated..."));
	return fNSAPINotify(channel.c_str(), command.c_str(), code, result.c_str(), result.size());
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
NSCAPI::nagiosReturn nscapi::core_wrapper::InjectSimpleCommand(const std::wstring command, const std::list<std::wstring> argument, std::wstring & msg, std::wstring & perf) 
{
	if (!fNSAPIInject)
		throw nscapi::nscapi_exception(_T("NSCore has not been initiated..."));

	std::string response;
	NSCAPI::nagiosReturn ret = InjectCommand(command, argument, response);
	if (!response.empty()) {
		PluginCommand::ResponseMessage rsp_msg;
		rsp_msg.ParseFromString(response);
		if (rsp_msg.payload_size() != 1) {
			CORE_LOG_ERROR_STD(_T("Failed to extract return message not 1 payload: ") + strEx::itos(rsp_msg.payload_size()));
			return NSCAPI::returnUNKNOWN;
		}
		::PluginCommand::Response payload = rsp_msg.payload(0);
		msg = to_wstring(payload.message());
		CORE_LOG_ERROR_STD(_T("Attempting to parse perf data"));
		perf = to_wstring(::nscapi::functions::build_performance_data(payload));
		CORE_LOG_ERROR_STD(_T("Attempting to parse perf data: '") + perf + _T("'"));
	}
	return ret;
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
NSCAPI::nagiosReturn nscapi::core_wrapper::InjectCommand(const std::wstring command, const std::list<std::wstring> argument, std::string & result) 
{
	if (!fNSAPIInject)
		throw nscapi::nscapi_exception(_T("NSCore has not been initiated..."));


	PluginCommand::RequestMessage message;
	PluginCommand::Header *hdr = message.mutable_header();
	hdr->set_type(PluginCommand::Header_Type_REQUEST);
	hdr->set_version(PluginCommand::Header_Version_VERSION_1);

	PluginCommand::Request *req = message.add_payload();
	req->set_command(to_string(command));
	req->set_version(PluginCommand::Request_Version_VERSION_1);

	BOOST_FOREACH(std::wstring s, argument)
		req->add_arguments(to_string(s));

	std::string request;
	message.SerializeToString(&request);

	char *buffer = NULL;
	unsigned int buffer_size = 0;

	NSCAPI::nagiosReturn retC = InjectCommandRAW(command.c_str(), request.c_str(), request.size(), &buffer, &buffer_size);

	if (buffer_size > 0 && buffer != NULL) {
		PluginCommand::ResponseMessage rsp_msg;
		result = std::string(buffer, buffer_size);
	}

	DestroyBuffer(&buffer);
	switch (retC) {
		case NSCAPI::returnIgnored:
			CORE_LOG_MESSAGE_STD(_T("No handler for command '") + command + _T("'."));
			break;
		case NSCAPI::returnOK:
		case NSCAPI::returnCRIT:
		case NSCAPI::returnWARN:
		case NSCAPI::returnUNKNOWN:
			break;
		default:
			throw nscapi::nscapi_exception(_T("Unknown return code when injecting: ") + std::wstring(command));
	}
	return retC;
}

/**
 * A wrapper around the InjetCommand that is simpler to use.
 * Parses a string by splitting and makes the array and also manages return buffers and such.
 * @param command The command to execute
 * @param buffer The buffer to split
 * @param spliwchar_t The char to use as splitter
 * @param message The return message buffer
 * @param perf The return performance data buffer
 * @return The result of the command
 */
NSCAPI::nagiosReturn nscapi::core_wrapper::InjectSplitAndCommand(const wchar_t* command, wchar_t* buffer, wchar_t splitChar, std::wstring & message, std::wstring & perf)
{
	if (!fNSAPIInject)
		throw nscapi::nscapi_exception(_T("NSCore has not been initiated..."));

	std::wstring args = std::wstring(buffer);
	boost::tokenizer<boost::escaped_list_separator<wchar_t>, std::wstring::const_iterator, std::wstring > tok(args, boost::escaped_list_separator<wchar_t>(L'\\', splitChar, L'\"'));
	std::list<std::wstring> arglist;
	BOOST_FOREACH(std::wstring s, tok)
		arglist.push_back(s);
	return InjectSimpleCommand(command, arglist, message, perf);
}
/**
 * A wrapper around the InjetCommand that is simpler to use.
 * @param command The command to execute
 * @param buffer The buffer to split
 * @param spliwchar_t The char to use as splitter
 * @param message The return message buffer
 * @param perf The return performance data buffer
 * @return The result of the command
 */
NSCAPI::nagiosReturn nscapi::core_wrapper::InjectSplitAndCommand(const std::wstring command, const std::wstring buffer, wchar_t spliwchar_t, std::wstring & message, std::wstring & perf, bool escape) {
	if (!fNSAPIInject)
		throw nscapi::nscapi_exception(_T("NSCore has not been initiated..."));
	std::list<std::wstring> arglist;
	if (escape) {
		boost::tokenizer<boost::escaped_list_separator<wchar_t>, std::wstring::const_iterator, std::wstring > tok(buffer, boost::escaped_list_separator<wchar_t>(L'\\', spliwchar_t, L'\"'));
		BOOST_FOREACH(std::wstring s, tok)
			arglist.push_back(s);
	} else {
		std::wstring split;
		split.push_back(spliwchar_t);
		boost::tokenizer<boost::escaped_list_separator<wchar_t>, std::wstring::const_iterator, std::wstring > tok(buffer, boost::escaped_list_separator<wchar_t>(_T(""), split, _T("\"")));
		BOOST_FOREACH(std::wstring s, tok)
			arglist.push_back(s);
	}
	return InjectSimpleCommand(command.c_str(), arglist, message, perf);
}

NSCAPI::nagiosReturn nscapi::core_wrapper::InjectNRPECommand(const std::wstring command, const std::wstring buffer, std::wstring & message, std::wstring & perf) {
	if (!fNSAPIInject)
		throw nscapi::nscapi_exception(_T("NSCore has not been initiated..."));
	boost::tokenizer<boost::char_separator<wchar_t>, std::wstring::const_iterator, std::wstring > tok(buffer, boost::char_separator<wchar_t>(_T("!")));
	std::list<std::wstring> arglist;
	BOOST_FOREACH(std::wstring s, tok)
		arglist.push_back(s);
	return InjectSimpleCommand(command.c_str(), arglist, message, perf);
}


/**
 * Ask the core to shutdown (only works when run as a service, o/w does nothing ?
 * @todo Check if this might cause damage if not run as a service.
 */
void nscapi::core_wrapper::StopService(void) {
	if (fNSAPIStopServer)
		fNSAPIStopServer();
}
/**
 * Close the program (usefull for tray/testmode) without stopping the service (unless this is the service).
 * @author mickem
 */
void nscapi::core_wrapper::Exit(void) {
	if (fNSAPIExit)
		fNSAPIExit();
}
/**
 * Retrieve a string from the settings subsystem (INI-file)
 * Might possibly be located in the registry in the future.
 *
 * @param section Section key (generally module specific, make sure this is "unique")
 * @param key The key to retrieve 
 * @param defaultValue A default value (if no value is set in the settings file)
 * @return the current value or defaultValue if no value is set.
 * @throws nscapi::nscapi_exception When core pointer set is unavailable or an error occurs.
 */
std::wstring nscapi::core_wrapper::getSettingsString(std::wstring section, std::wstring key, std::wstring defaultValue) {
	if (!fNSAPIGetSettingsString)
		throw nscapi::nscapi_exception(_T("NSCore has not been initiated..."));
	unsigned int buf_len = getBufferLength();
	wchar_t *buffer = new wchar_t[buf_len+1];
	if (fNSAPIGetSettingsString(section.c_str(), key.c_str(), defaultValue.c_str(), buffer, buf_len) != NSCAPI::isSuccess) {
		delete [] buffer;
		throw nscapi::nscapi_exception(_T("Settings could not be retrieved."));
	}
	std::wstring ret = buffer;
	delete [] buffer;
	return ret;
}

std::wstring nscapi::core_wrapper::expand_path(std::wstring value) {
	if (!fNSAPIExpandPath)
		throw nscapi::nscapi_exception(_T("NSCore has not been initiated..."));
	unsigned int buf_len = getBufferLength();
	wchar_t *buffer = new wchar_t[buf_len+1];
	if (fNSAPIExpandPath(value.c_str(), buffer, buf_len) != NSCAPI::isSuccess) {
		delete [] buffer;
		throw nscapi::nscapi_exception(_T("Settings could not be retrieved."));
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
std::list<std::wstring> nscapi::core_wrapper::getSettingsSection(std::wstring section) {
	if (!fNSAPIGetSettingsSection)
		throw nscapi::nscapi_exception(_T("NSCore has not been initiated..."));
	arrayBuffer::arrayBuffer aBuffer = NULL;
	unsigned int argLen = 0;
	if (fNSAPIGetSettingsSection(section.c_str(), &aBuffer, &argLen) != NSCAPI::isSuccess) {
		throw nscapi::nscapi_exception(_T("Settings could not be retrieved."));
	}
	std::list<std::wstring> ret = arrayBuffer::arrayBuffer2list(argLen, aBuffer);
	if (fNSAPIReleaseSettingsSectionBuffer(&aBuffer, &argLen) != NSCAPI::isSuccess) {
		throw nscapi::nscapi_exception(_T("Settings could not be destroyed."));
	}
	if (aBuffer != NULL)
		throw nscapi::nscapi_exception(_T("buffer is not null?."));
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
 * @throws nscapi::nscapi_exception When core pointer set is unavailable.
 */
int nscapi::core_wrapper::getSettingsInt(std::wstring section, std::wstring key, int defaultValue) {
	if (!fNSAPIGetSettingsInt)
		throw nscapi::nscapi_exception(_T("NSCore has not been initiated..."));
	return fNSAPIGetSettingsInt(section.c_str(), key.c_str(), defaultValue);
}
bool nscapi::core_wrapper::getSettingsBool(std::wstring section, std::wstring key, bool defaultValue) {
	if (!fNSAPIGetSettingsBool)
		throw nscapi::nscapi_exception(_T("NSCore has not been initiated..."));
	return fNSAPIGetSettingsBool(section.c_str(), key.c_str(), defaultValue?1:0) == 1;
}
void nscapi::core_wrapper::settings_register_key(std::wstring path, std::wstring key, NSCAPI::settings_type type, std::wstring title, std::wstring description, std::wstring defaultValue, bool advanced) {
	if (!fNSAPISettingsRegKey)
		throw nscapi::nscapi_exception(_T("NSCore has not been initiated..."));
	fNSAPISettingsRegKey(path.c_str(), key.c_str(), type, title.c_str(), description.c_str(), defaultValue.c_str(), advanced);
}
void nscapi::core_wrapper::settings_register_path(std::wstring path, std::wstring title, std::wstring description, bool advanced) {
	if (!fNSAPISettingsRegPath)
		throw nscapi::nscapi_exception(_T("NSCore has not been initiated..."));
	fNSAPISettingsRegPath(path.c_str(), title.c_str(), description.c_str(), advanced);
}


void nscapi::core_wrapper::settings_save() {
	if (!fNSAPISettingsSave)
		throw nscapi::nscapi_exception(_T("NSCore has not been initiated..."));
	fNSAPISettingsSave();
}

/**
 * Retrieve the application name (in human readable format) from the core.
 * @return A string representing the application name.
 * @throws nscapi::nscapi_exception When core pointer set is unavailable or an unexpected error occurs.
 */
std::wstring nscapi::core_wrapper::getApplicationName() {
	if (!fNSAPIGetApplicationName)
		throw nscapi::nscapi_exception(_T("NSCore has not been initiated..."));
	unsigned int buf_len = getBufferLength();
	wchar_t *buffer = new wchar_t[buf_len+1];
	if (fNSAPIGetApplicationName(buffer, buf_len) != NSCAPI::isSuccess) {
		delete [] buffer;
		throw nscapi::nscapi_exception(_T("Application name could not be retrieved"));
	}
	std::wstring ret = buffer;
	delete [] buffer;
	return ret;
}
/**
 * Retrieve the directory root of the application from the core.
 * @return A string representing the base path.
 * @throws nscapi::nscapi_exception When core pointer set is unavailable or an unexpected error occurs.
 */
std::wstring nscapi::core_wrapper::getBasePath() {
	if (!fNSAPIGetBasePath)
		throw nscapi::nscapi_exception(_T("NSCore has not been initiated..."));
	unsigned int buf_len = getBufferLength();
	wchar_t *buffer = new wchar_t[buf_len+1];
	if (fNSAPIGetBasePath(buffer, buf_len) != NSCAPI::isSuccess) {
		delete [] buffer;
		throw nscapi::nscapi_exception(_T("Base path could not be retrieved"));
	}
	std::wstring ret = buffer;
	delete [] buffer;
	return ret;
}

unsigned int nscapi::core_wrapper::getBufferLength() {
	static unsigned int len = 0;
	if (len == 0) {
		len = getSettingsInt(setting_keys::settings_def::PAYLOAD_LEN_PATH, setting_keys::settings_def::PAYLOAD_LEN, setting_keys::settings_def::PAYLOAD_LEN_DEFAULT);
	}
	return len;
}


bool nscapi::core_wrapper::logDebug() {
	enum status {unknown, debug, nodebug };
	static status d = unknown;
	if (d == unknown) {
		if (checkLogMessages(debug)== NSCAPI::istrue)
			d = debug;
		else
			d = nodebug;
	}
	return (d == debug);
}

std::wstring nscapi::core_wrapper::Encrypt(std::wstring str, unsigned int algorithm) {
	if (!fNSAPIEncrypt)
		throw nscapi::nscapi_exception(_T("NSCore has not been initiated..."));
	unsigned int len = 0;
	// @todo investigate potential problems with static_cast<unsigned int>
	fNSAPIEncrypt(algorithm, str.c_str(), static_cast<unsigned int>(str.size()), NULL, &len);
	len+=2;
	wchar_t *buf = new wchar_t[len+1];
	NSCAPI::errorReturn ret = fNSAPIEncrypt(algorithm, str.c_str(), static_cast<unsigned int>(str.size()), buf, &len);
	if (ret == NSCAPI::isSuccess) {
		std::wstring ret = buf;
		delete [] buf;
		return ret;
	}
	return _T("");
}
std::wstring nscapi::core_wrapper::Decrypt(std::wstring str, unsigned int algorithm) {
	if (!fNSAPIDecrypt)
		throw nscapi::nscapi_exception(_T("NSCore has not been initiated..."));
	unsigned int len = 0;
	// @todo investigate potential problems with: static_cast<unsigned int>(str.size())
	fNSAPIDecrypt(algorithm, str.c_str(), static_cast<unsigned int>(str.size()), NULL, &len);
	len+=2;
	wchar_t *buf = new wchar_t[len+1];
	NSCAPI::errorReturn ret = fNSAPIDecrypt(algorithm, str.c_str(), static_cast<unsigned int>(str.size()), buf, &len);
	if (ret == NSCAPI::isSuccess) {
		std::wstring ret = buf;
		delete [] buf;
		return ret;
	}
	return _T("");
}
NSCAPI::errorReturn nscapi::core_wrapper::SetSettingsString(std::wstring section, std::wstring key, std::wstring value) {
	if (!fNSAPISetSettingsString)
		throw nscapi::nscapi_exception(_T("NSCore has not been initiated..."));
	return fNSAPISetSettingsString(section.c_str(), key.c_str(), value.c_str());
}
NSCAPI::errorReturn nscapi::core_wrapper::SetSettingsInt(std::wstring section, std::wstring key, int value) {
	if (!fNSAPISetSettingsInt)
		throw nscapi::nscapi_exception(_T("NSCore has not been initiated..."));
	return fNSAPISetSettingsInt(section.c_str(), key.c_str(), value);
}
NSCAPI::errorReturn nscapi::core_wrapper::WriteSettings(int type) {
	if (!fNSAPIWriteSettings)
		throw nscapi::nscapi_exception(_T("NSCore has not been initiated..."));
	return fNSAPIWriteSettings(type);
}
NSCAPI::errorReturn nscapi::core_wrapper::ReadSettings(int type) {
	if (!fNSAPIReadSettings)
		throw nscapi::nscapi_exception(_T("NSCore has not been initiated..."));
	return fNSAPIReadSettings(type);
}
NSCAPI::errorReturn nscapi::core_wrapper::Rehash(int flag) {
	if (!fNSAPIRehash)
		throw nscapi::nscapi_exception(_T("NSCore has not been initiated..."));
	return fNSAPIRehash(flag);
}
nscapi::core_wrapper::plugin_info_list nscapi::core_wrapper::getPluginList() {
	if (!fNSAPIGetPluginList || !fNSAPIReleasePluginList)
		throw nscapi::nscapi_exception(_T("NSCore has not been initiated..."));
	plugin_info_list ret;
	
	
	int len = 0;
	//NSCAPI::plugin_info_list **list2;
	//NSCAPI::plugin_info_list *list[1];
	NSCAPI::plugin_info *list[1];
	//typedef NSCAPI::errorReturn (*lpNSAPIGetPluginList)(int *len, NSAPI_plugin_info** list);
	//typedef NSCAPI::errorReturn (*lpNSAPIReleasePluginList)(int len, NSAPI_plugin_info** list);
	NSCAPI::errorReturn err = fNSAPIGetPluginList(&len, list);
	if (err != NSCAPI::isSuccess)
		return ret;
	for (int i=0;i<len;i++) {
		plugin_info_type info;
		info.description = (*list)[i].description;
		info.name = (*list)[i].name;
		info.dll = (*list)[i].dll;
		ret.push_back(info);
	}
	fNSAPIReleasePluginList(len, list);
	return ret;
}

std::list<std::wstring> nscapi::core_wrapper::getAllCommandNames() {
	if (!fNSAPIGetAllCommandNames || !fNSAPIReleaseAllCommandNamessBuffer )
		throw nscapi::nscapi_exception(_T("NSCore has not been initiated..."));
	arrayBuffer::arrayBuffer aBuffer = NULL;
	unsigned int argLen = 0;
	if (fNSAPIGetAllCommandNames(&aBuffer, &argLen) != NSCAPI::isSuccess) {
		throw nscapi::nscapi_exception(_T("Commands could not be retrieved."));
	}
	std::list<std::wstring> ret = arrayBuffer::arrayBuffer2list(argLen, aBuffer);
	if (fNSAPIReleaseAllCommandNamessBuffer(&aBuffer, &argLen) != NSCAPI::isSuccess) {
		throw nscapi::nscapi_exception(_T("Commands could not be destroyed."));
	}
	if (aBuffer != NULL)
		throw nscapi::nscapi_exception(_T("buffer is not null?."));
	return ret;
}
std::wstring nscapi::core_wrapper::describeCommand(std::wstring command) {
	if (!fNSAPIDescribeCommand)
		throw nscapi::nscapi_exception(_T("NSCore has not been initiated..."));
	unsigned int buf_len = getBufferLength();
	wchar_t *buffer = new wchar_t[buf_len+1];
	if (fNSAPIDescribeCommand(command.c_str(), buffer, buf_len) != NSCAPI::isSuccess) {
		delete [] buffer;
		throw nscapi::nscapi_exception(_T("Base path could not be retrieved"));
	}
	std::wstring ret = buffer;
	delete [] buffer;
	return ret;
}
void nscapi::core_wrapper::registerCommand(std::wstring command, std::wstring description) {
	if (!fNSAPIRegisterCommand)
		throw nscapi::nscapi_exception(_T("NSCore has not been initiated..."));
	if (fNSAPIRegisterCommand(id_, command.c_str(), description.c_str()) != NSCAPI::isSuccess) {
		CORE_LOG_ERROR_STD(_T("Failed to register command: ") + command + _T(" in plugin: ") + to_wstring(id_));
	}
}


bool nscapi::core_wrapper::checkLogMessages(int type) {
	if (!fNSAPICheckLogMessages)
		throw nscapi::nscapi_exception(_T("NSCore has not been initiated..."));
	return fNSAPICheckLogMessages(type) == NSCAPI::istrue;
}
/**
 * Retrieve the application version as a string (in human readable format) from the core.
 * @return A string representing the application version.
 * @throws nscapi::nscapi_exception When core pointer set is unavailable.
 */
std::wstring nscapi::core_wrapper::getApplicationVersionString() {
	if (!fNSAPIGetApplicationVersionStr)
		throw nscapi::nscapi_exception(_T("NSCore has not been initiated..."));
	unsigned int buf_len = getBufferLength();
	wchar_t *buffer = new wchar_t[buf_len+1];
	if (fNSAPIGetApplicationVersionStr(buffer, buf_len) != NSCAPI::isSuccess) {
		delete [] buffer;
		return _T("");
	}
	std::wstring ret = buffer;
	delete [] buffer;
	return ret;
}

/**
 * Wrapper function around the ModuleHelperInit call.
 * This wrapper retrieves all pointers and stores them for future use.
 * @param f A function pointer to a function that can be used to load function from the core.
 * @return NSCAPI::success or NSCAPI::failure
 */
bool nscapi::core_wrapper::load_endpoints(unsigned int id, nscapi::core_api::lpNSAPILoader f) {
	id_ = id;
	fNSAPIGetApplicationName = (nscapi::core_api::lpNSAPIGetApplicationName)f(_T("NSAPIGetApplicationName"));
	fNSAPIGetApplicationVersionStr = (nscapi::core_api::lpNSAPIGetApplicationVersionStr)f(_T("NSAPIGetApplicationVersionStr"));
	fNSAPIGetSettingsInt = (nscapi::core_api::lpNSAPIGetSettingsInt)f(_T("NSAPIGetSettingsInt"));
	fNSAPIGetSettingsBool = (nscapi::core_api::lpNSAPIGetSettingsBool)f(_T("NSAPIGetSettingsBool"));
	fNSAPIGetSettingsString = (nscapi::core_api::lpNSAPIGetSettingsString)f(_T("NSAPIGetSettingsString"));
	fNSAPIGetSettingsSection = (nscapi::core_api::lpNSAPIGetSettingsSection)f(_T("NSAPIGetSettingsSection"));
	fNSAPIReleaseSettingsSectionBuffer = (nscapi::core_api::lpNSAPIReleaseSettingsSectionBuffer)f(_T("NSAPIReleaseSettingsSectionBuffer"));
	fNSAPIMessage = (nscapi::core_api::lpNSAPIMessage)f(_T("NSAPIMessage"));
	fNSAPIStopServer = (nscapi::core_api::lpNSAPIStopServer)f(_T("NSAPIStopServer"));
	//fNSAPIExit = (nscapi::core_api::lpNSAPIExit)f(_T("NSAPIExit"));
	fNSAPIInject = (nscapi::core_api::lpNSAPIInject)f(_T("NSAPIInject"));
	fNSAPIDestroyBuffer = (nscapi::core_api::lpNSAPIDestroyBuffer)f(_T("NSAPIDestroyBuffer"));
	fNSAPINotify = (nscapi::core_api::lpNSAPINotify)f(_T("NSAPINotify"));
	fNSAPIGetBasePath = (nscapi::core_api::lpNSAPIGetBasePath)f(_T("NSAPIGetBasePath"));
	fNSAPICheckLogMessages = (nscapi::core_api::lpNSAPICheckLogMessages)f(_T("NSAPICheckLogMessages"));
	fNSAPIDecrypt = (nscapi::core_api::lpNSAPIDecrypt)f(_T("NSAPIDecrypt"));
	fNSAPIEncrypt = (nscapi::core_api::lpNSAPIEncrypt)f(_T("NSAPIEncrypt"));
	fNSAPISetSettingsString = (nscapi::core_api::lpNSAPISetSettingsString)f(_T("NSAPISetSettingsString"));
	fNSAPISetSettingsInt = (nscapi::core_api::lpNSAPISetSettingsInt)f(_T("NSAPISetSettingsInt"));
	fNSAPIWriteSettings = (nscapi::core_api::lpNSAPIWriteSettings)f(_T("NSAPIWriteSettings"));
	fNSAPIReadSettings = (nscapi::core_api::lpNSAPIReadSettings)f(_T("NSAPIReadSettings"));
	fNSAPIRehash = (nscapi::core_api::lpNSAPIRehash)f(_T("NSAPIRehash"));

	fNSAPIDescribeCommand = (nscapi::core_api::lpNSAPIDescribeCommand)f(_T("NSAPIDescribeCommand"));
	fNSAPIGetAllCommandNames = (nscapi::core_api::lpNSAPIGetAllCommandNames)f(_T("NSAPIGetAllCommandNames"));
	fNSAPIReleaseAllCommandNamessBuffer = (nscapi::core_api::lpNSAPIReleaseAllCommandNamessBuffer)f(_T("NSAPIReleaseAllCommandNamessBuffer"));
	fNSAPIRegisterCommand = (nscapi::core_api::lpNSAPIRegisterCommand)f(_T("NSAPIRegisterCommand"));

	fNSAPISettingsRegKey = (nscapi::core_api::lpNSAPISettingsRegKey)f(_T("NSAPISettingsRegKey"));
	fNSAPISettingsRegPath = (nscapi::core_api::lpNSAPISettingsRegPath)f(_T("NSAPISettingsRegPath"));

	fNSAPIGetPluginList = (nscapi::core_api::lpNSAPIGetPluginList)f(_T("NSAPIGetPluginList"));
	fNSAPIReleasePluginList = (nscapi::core_api::lpNSAPIReleasePluginList)f(_T("NSAPIReleasePluginList"));

	fNSAPISettingsSave = (nscapi::core_api::lpNSAPISettingsSave)f(_T("NSAPISettingsSave"));

	fNSAPIExpandPath = (nscapi::core_api::lpNSAPIExpandPath)f(_T("NSAPIExpandPath"));
	
	return true;
}

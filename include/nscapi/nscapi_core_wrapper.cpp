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

#include <iostream>

#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/nscapi_helper.hpp>

#include <strEx.h>
#include <arrayBuffer.h>

//#include <protobuf/plugin.pb.h>
//#include <nscapi/nscapi_protobuf_functions.hpp>

#define CORE_LOG_ERROR_STD(msg) if (should_log(NSCAPI::log_level::error)) { log(NSCAPI::log_level::error, __FILE__, __LINE__, (std::wstring)msg); }
#define CORE_LOG_ERROR(msg) if (should_log(NSCAPI::log_level::error)) { log(NSCAPI::log_level::error, __FILE__, __LINE__, msg); }

#define LEGACY_BUFFER_LENGTH 4096
//////////////////////////////////////////////////////////////////////////
// Callbacks into the core
//////////////////////////////////////////////////////////////////////////

bool nscapi::core_wrapper::should_log(NSCAPI::nagiosReturn msgType) {
	enum log_status {unknown, set };
	static NSCAPI::log_level::level level = NSCAPI::log_level::info;
	static log_status status = unknown;
	if (status == unknown) {
		level = get_loglevel();
		status = set;
	}
	return nscapi::logging::matches(level, msgType);
}


/**
 * Callback to send a message through to the core
 *
 * @param msgType Message type (debug, warning, etc.)
 * @param file File where message was generated (__FILE__)
 * @param line Line where message was generated (__LINE__)
 * @param message Message in human readable format
 * @throws nscapi::nscapi_exception When core pointer set is unavailable.
 */
void nscapi::core_wrapper::log(NSCAPI::nagiosReturn msgType, std::string file, int line, std::wstring logMessage) {
	if (!should_log(msgType))
		return;
	if (!fNSAPISimpleMessage) {
		std::wcout << _T("NSCORE NOT LOADED Dumping log: ") << line << _T(": ") << std::endl << logMessage << std::endl;
		return;
	}
	std::string str;
	try {
		/*
		Plugin::LogEntry message;
		Plugin::LogEntry::Entry *msg = message.add_entry();
		msg->set_level(nscapi::functions::log_to_gpb(msgType));
		msg->set_file(file);
		msg->set_line(line);
		msg->set_message(utf8::cvt<std::string>(logMessage));
		if (!message.SerializeToString(&str)) {
			std::wcout << _T("Failed to generate message: SERIALIZATION ERROR");
		}
		*/
		return fNSAPISimpleMessage(msgType, file.c_str(), line, logMessage.c_str());
	} catch (const std::exception &e) {
		std::wcout << _T("Failed to generate message: ") << utf8::to_unicode(e.what());
	} catch (...) {
		std::wcout << _T("Failed to generate message: UNKNOWN");
	}
}
void nscapi::core_wrapper::log(NSCAPI::nagiosReturn msgType, std::string file, int line, std::string message) {
	if (!should_log(msgType))
		return;
	log(msgType, file, line, utf8::cvt<std::wstring>(message));
}

NSCAPI::log_level::level nscapi::core_wrapper::get_loglevel() {
	if (!fNSAPIGetLoglevel) {
		return NSCAPI::log_level::debug;
	}
	return fNSAPIGetLoglevel();
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
NSCAPI::nagiosReturn nscapi::core_wrapper::query(const wchar_t* command, const char *request, const unsigned int request_len, char **response, unsigned int *response_len) 
{
	if (!fNSAPIInject)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	return fNSAPIInject(command, request, request_len, response, response_len);
}


void nscapi::core_wrapper::DestroyBuffer(char**buffer) {
	if (!fNSAPIDestroyBuffer)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	return fNSAPIDestroyBuffer(buffer);
}


NSCAPI::errorReturn nscapi::core_wrapper::submit_message(std::wstring channel, std::string request, std::string &response) {

	if (!fNSAPINotify)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	char *buffer = NULL;
	unsigned int buffer_size = 0;
	NSCAPI::nagiosReturn ret = submit_message(channel.c_str(), request.c_str(), request.size(), &buffer, &buffer_size);

	if (buffer_size > 0 && buffer != NULL) {
		response = std::string(buffer, buffer_size);
	}

	DestroyBuffer(&buffer);
	return ret;
}

NSCAPI::errorReturn nscapi::core_wrapper::reload(std::wstring module) {

	if (!fNSAPIReload)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	return fNSAPIReload(module.c_str());
}
NSCAPI::nagiosReturn nscapi::core_wrapper::submit_message(const wchar_t* channel, const char *request, const unsigned int request_len, char **response, unsigned int *response_len) 
{
	if (!fNSAPINotify)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	return fNSAPINotify(channel, request, request_len, response, response_len);
}

NSCAPI::nagiosReturn nscapi::core_wrapper::query(const std::wstring & command, const std::string & request, std::string & result) 
{
	if (!fNSAPIInject)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	char *buffer = NULL;
	unsigned int buffer_size = 0;
	NSCAPI::nagiosReturn retC = query(command.c_str(), request.c_str(), request.size(), &buffer, &buffer_size);

	if (buffer_size > 0 && buffer != NULL) {
		//PluginCommand::ResponseMessage rsp_msg;
		result = std::string(buffer, buffer_size);
	}

	DestroyBuffer(&buffer);
	switch (retC) {
		case NSCAPI::returnIgnored:
			CORE_LOG_ERROR_STD(_T("No handler for command '") + command + _T("'."));
			break;
		case NSCAPI::returnOK:
		case NSCAPI::returnCRIT:
		case NSCAPI::returnWARN:
		case NSCAPI::returnUNKNOWN:
			break;
		default:
			throw nscapi::nscapi_exception("Unknown return code from query: " + utf8::cvt<std::string>(command));
	}
	return retC;
}

NSCAPI::nagiosReturn nscapi::core_wrapper::exec_command(const std::wstring target, const std::wstring command, std::string request, std::string & result) {
	char *buffer = NULL;
	unsigned int buffer_size = 0;
	NSCAPI::nagiosReturn retC = exec_command(target.c_str(), command.c_str(), request.c_str(), request.size(), &buffer, &buffer_size);

	if (buffer_size > 0 && buffer != NULL) {
		result = std::string(buffer, buffer_size);
	}

	DestroyBuffer(&buffer);
	switch (retC) {
		case NSCAPI::returnIgnored:
			CORE_LOG_ERROR_STD(_T("No handler for command '") + command + _T("'."));
			break;
		case NSCAPI::returnOK:
		case NSCAPI::returnCRIT:
		case NSCAPI::returnWARN:
		case NSCAPI::returnUNKNOWN:
			break;
		default:
			throw nscapi::nscapi_exception("Unknown return from exec: " + utf8::cvt<std::string>(command));
	}
	return retC;
}
NSCAPI::nagiosReturn nscapi::core_wrapper::exec_command(const wchar_t* target, const wchar_t* command, const char *request, const unsigned int request_len, char **response, unsigned int *response_len) 
{
	if (!fNSAPIExecCommand)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	return fNSAPIExecCommand(target, command, request, request_len, response, response_len);
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
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	unsigned int buf_len = LEGACY_BUFFER_LENGTH;
	wchar_t *buffer = new wchar_t[buf_len+1];
	if (fNSAPIGetSettingsString(section.c_str(), key.c_str(), defaultValue.c_str(), buffer, buf_len) != NSCAPI::isSuccess) {
		delete [] buffer;
		throw nscapi::nscapi_exception("Settings could not be retrieved.");
	}
	std::wstring ret = buffer;
	delete [] buffer;
	return ret;
}

std::wstring nscapi::core_wrapper::expand_path(std::wstring value) {
	if (!fNSAPIExpandPath)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	unsigned int buf_len = LEGACY_BUFFER_LENGTH;
	wchar_t *buffer = new wchar_t[buf_len+1];
	if (fNSAPIExpandPath(value.c_str(), buffer, buf_len) != NSCAPI::isSuccess) {
		delete [] buffer;
		throw nscapi::nscapi_exception("Settings could not be retrieved.");
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
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	array_buffer::arrayBuffer aBuffer = NULL;
	unsigned int argLen = 0;
	if (fNSAPIGetSettingsSection(section.c_str(), &aBuffer, &argLen) != NSCAPI::isSuccess) {
		throw nscapi::nscapi_exception("Settings could not be retrieved.");
	}
	std::list<std::wstring> ret = array_buffer::arrayBuffer2list(argLen, aBuffer);
	if (fNSAPIReleaseSettingsSectionBuffer(&aBuffer, &argLen) != NSCAPI::isSuccess) {
		throw nscapi::nscapi_exception("Settings could not be destroyed.");
	}
	if (aBuffer != NULL)
		throw nscapi::nscapi_exception("buffer is not null?.");
	return ret;
}
std::list<std::wstring> nscapi::core_wrapper::getSettingsSections(std::wstring section) {
	if (!fNSAPIGetSettingsSections)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	array_buffer::arrayBuffer aBuffer = NULL;
	unsigned int argLen = 0;
	if (fNSAPIGetSettingsSections(section.c_str(), &aBuffer, &argLen) != NSCAPI::isSuccess) {
		throw nscapi::nscapi_exception("Settings could not be retrieved.");
	}
	std::list<std::wstring> ret = array_buffer::arrayBuffer2list(argLen, aBuffer);
	if (fNSAPIReleaseSettingsSectionBuffer(&aBuffer, &argLen) != NSCAPI::isSuccess) {
		throw nscapi::nscapi_exception("Settings could not be destroyed.");
	}
	if (aBuffer != NULL)
		throw nscapi::nscapi_exception("buffer is not null?.");
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
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	return fNSAPIGetSettingsInt(section.c_str(), key.c_str(), defaultValue);
}
bool nscapi::core_wrapper::getSettingsBool(std::wstring section, std::wstring key, bool defaultValue) {
	if (!fNSAPIGetSettingsBool)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	return fNSAPIGetSettingsBool(section.c_str(), key.c_str(), defaultValue?1:0) == 1;
}
void nscapi::core_wrapper::settings_register_key(std::wstring path, std::wstring key, NSCAPI::settings_type type, std::wstring title, std::wstring description, std::wstring defaultValue, bool advanced) {
	if (!fNSAPISettingsRegKey)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	fNSAPISettingsRegKey(path.c_str(), key.c_str(), type, title.c_str(), description.c_str(), defaultValue.c_str(), advanced);
}
void nscapi::core_wrapper::settings_register_path(std::wstring path, std::wstring title, std::wstring description, bool advanced) {
	if (!fNSAPISettingsRegPath)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	fNSAPISettingsRegPath(path.c_str(), title.c_str(), description.c_str(), advanced);
}


void nscapi::core_wrapper::settings_save() {
	if (!fNSAPISettingsSave)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	fNSAPISettingsSave();
}

/**
 * Retrieve the application name (in human readable format) from the core.
 * @return A string representing the application name.
 * @throws nscapi::nscapi_exception When core pointer set is unavailable or an unexpected error occurs.
 */
std::wstring nscapi::core_wrapper::getApplicationName() {
	if (!fNSAPIGetApplicationName)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	unsigned int buf_len = LEGACY_BUFFER_LENGTH;
	wchar_t *buffer = new wchar_t[buf_len+1];
	if (fNSAPIGetApplicationName(buffer, buf_len) != NSCAPI::isSuccess) {
		delete [] buffer;
		throw nscapi::nscapi_exception("Application name could not be retrieved");
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
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	unsigned int buf_len = LEGACY_BUFFER_LENGTH;
	wchar_t *buffer = new wchar_t[buf_len+1];
	if (fNSAPIGetBasePath(buffer, buf_len) != NSCAPI::isSuccess) {
		delete [] buffer;
		throw nscapi::nscapi_exception("Base path could not be retrieved");
	}
	std::wstring ret = buffer;
	delete [] buffer;
	return ret;
}

std::wstring nscapi::core_wrapper::Encrypt(std::wstring str, unsigned int algorithm) {
	if (!fNSAPIEncrypt)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
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
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
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
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	return fNSAPISetSettingsString(section.c_str(), key.c_str(), value.c_str());
}
NSCAPI::errorReturn nscapi::core_wrapper::SetSettingsInt(std::wstring section, std::wstring key, int value) {
	if (!fNSAPISetSettingsInt)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	return fNSAPISetSettingsInt(section.c_str(), key.c_str(), value);
}
NSCAPI::errorReturn nscapi::core_wrapper::WriteSettings(int type) {
	if (!fNSAPIWriteSettings)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	return fNSAPIWriteSettings(type);
}
NSCAPI::errorReturn nscapi::core_wrapper::ReadSettings(int type) {
	if (!fNSAPIReadSettings)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	return fNSAPIReadSettings(type);
}
NSCAPI::errorReturn nscapi::core_wrapper::Rehash(int flag) {
	if (!fNSAPIRehash)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	return fNSAPIRehash(flag);
}
nscapi::core_wrapper::plugin_info_list nscapi::core_wrapper::getPluginList() {
	if (!fNSAPIGetPluginList || !fNSAPIReleasePluginList)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
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
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	array_buffer::arrayBuffer aBuffer = NULL;
	unsigned int argLen = 0;
	if (fNSAPIGetAllCommandNames(&aBuffer, &argLen) != NSCAPI::isSuccess) {
		throw nscapi::nscapi_exception("Commands could not be retrieved.");
	}
	std::list<std::wstring> ret = array_buffer::arrayBuffer2list(argLen, aBuffer);
	if (fNSAPIReleaseAllCommandNamessBuffer(&aBuffer, &argLen) != NSCAPI::isSuccess) {
		throw nscapi::nscapi_exception("Commands could not be destroyed.");
	}
	if (aBuffer != NULL)
		throw nscapi::nscapi_exception("buffer is not null?.");
	return ret;
}
std::wstring nscapi::core_wrapper::describeCommand(std::wstring command) {
	if (!fNSAPIDescribeCommand)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	unsigned int buf_len = LEGACY_BUFFER_LENGTH;
	wchar_t *buffer = new wchar_t[buf_len+1];
	if (fNSAPIDescribeCommand(command.c_str(), buffer, buf_len) != NSCAPI::isSuccess) {
		delete [] buffer;
		throw nscapi::nscapi_exception("Base path could not be retrieved");
	}
	std::wstring ret = buffer;
	delete [] buffer;
	return ret;
}
void nscapi::core_wrapper::registerCommand(unsigned int id, std::wstring command, std::wstring description) {
	if (!fNSAPIRegisterCommand)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	if (fNSAPIRegisterCommand(id, command.c_str(), description.c_str()) != NSCAPI::isSuccess) {
		CORE_LOG_ERROR_STD(_T("Failed to register command: ") + command + _T(" in plugin: ") + strEx::itos(id));
	}
}

void nscapi::core_wrapper::registerSubmissionListener(unsigned int id, std::wstring channel) {
	if (!fNSAPIRegisterSubmissionListener)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	if (fNSAPIRegisterSubmissionListener(id, channel.c_str()) != NSCAPI::isSuccess) {
		CORE_LOG_ERROR_STD(_T("Failed to register channel: ") + channel + _T(" in plugin: ") + strEx::itos(id));
	}
}
void nscapi::core_wrapper::registerRoutingListener(unsigned int id, std::wstring channel) {
	if (!fNSAPIRegisterRoutingListener)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	if (fNSAPIRegisterRoutingListener(id, channel.c_str()) != NSCAPI::isSuccess) {
		CORE_LOG_ERROR_STD(_T("Failed to register channel: ") + channel + _T(" in plugin: ") + strEx::itos(id));
	}
}


bool nscapi::core_wrapper::checkLogMessages(int type) {
	if (!fNSAPICheckLogMessages)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	return fNSAPICheckLogMessages(type) == NSCAPI::istrue;
}
/**
 * Retrieve the application version as a string (in human readable format) from the core.
 * @return A string representing the application version.
 * @throws nscapi::nscapi_exception When core pointer set is unavailable.
 */
std::wstring nscapi::core_wrapper::getApplicationVersionString() {
	if (!fNSAPIGetApplicationVersionStr)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	unsigned int buf_len = LEGACY_BUFFER_LENGTH;
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
bool nscapi::core_wrapper::load_endpoints(nscapi::core_api::lpNSAPILoader f) {
	fNSAPIGetApplicationName = (nscapi::core_api::lpNSAPIGetApplicationName)f(_T("NSAPIGetApplicationName"));
	fNSAPIGetApplicationVersionStr = (nscapi::core_api::lpNSAPIGetApplicationVersionStr)f(_T("NSAPIGetApplicationVersionStr"));
	fNSAPIGetSettingsInt = (nscapi::core_api::lpNSAPIGetSettingsInt)f(_T("NSAPIGetSettingsInt"));
	fNSAPIGetSettingsBool = (nscapi::core_api::lpNSAPIGetSettingsBool)f(_T("NSAPIGetSettingsBool"));
	fNSAPIGetSettingsString = (nscapi::core_api::lpNSAPIGetSettingsString)f(_T("NSAPIGetSettingsString"));
	fNSAPIGetSettingsSection = (nscapi::core_api::lpNSAPIGetSettingsSection)f(_T("NSAPIGetSettingsSection"));
	fNSAPIGetSettingsSections = (nscapi::core_api::lpNSAPIGetSettingsSections)f(_T("NSAPIGetSettingsSections"));
	fNSAPIReleaseSettingsSectionBuffer = (nscapi::core_api::lpNSAPIReleaseSettingsSectionBuffer)f(_T("NSAPIReleaseSettingsSectionBuffer"));
	fNSAPIMessage = (nscapi::core_api::lpNSAPIMessage)f(_T("NSAPIMessage"));
	fNSAPISimpleMessage = (nscapi::core_api::lpNSAPISimpleMessage)f(_T("NSAPISimpleMessage"));
	fNSAPIStopServer = (nscapi::core_api::lpNSAPIStopServer)f(_T("NSAPIStopServer"));
	//fNSAPIExit = (nscapi::core_api::lpNSAPIExit)f(_T("NSAPIExit"));
	fNSAPIInject = (nscapi::core_api::lpNSAPIInject)f(_T("NSAPIInject"));
	fNSAPIExecCommand = (nscapi::core_api::lpNSAPIExecCommand)f(_T("NSAPIExecCommand"));
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
	fNSAPIReload = (nscapi::core_api::lpNSAPIReload)f(_T("NSAPIReload"));

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
	
	fNSAPIRegisterSubmissionListener = (nscapi::core_api::lpNSAPIRegisterSubmissionListener)f(_T("NSAPIRegisterSubmissionListener"));
	fNSAPIRegisterRoutingListener = (nscapi::core_api::lpNSAPIRegisterRoutingListener)f(_T("NSAPIRegisterRoutingListener"));
	fNSAPIGetLoglevel = (nscapi::core_api::lpNSAPIGetLoglevel)f(_T("NSAPIGetLoglevel"));

	return true;
}

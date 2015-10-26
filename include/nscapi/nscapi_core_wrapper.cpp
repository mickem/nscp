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

//#include <iostream>

#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/nscapi_helper.hpp>
#include <nscp_string.hpp>

#define CORE_LOG_ERROR_STD(msg) if (should_log(NSCAPI::log_level::error)) { log(NSCAPI::log_level::error, __FILE__, __LINE__, (std::string)msg); }
#define CORE_LOG_ERROR(msg) if (should_log(NSCAPI::log_level::error)) { log(NSCAPI::log_level::error, __FILE__, __LINE__, msg); }
#define CORE_LOG_ERROR_WA(msg, wa) if (should_log(NSCAPI::log_level::error)) { log(NSCAPI::log_level::error, __FILE__, __LINE__, msg + utf8::cvt<std::string>(wa)); }

#define LEGACY_BUFFER_LENGTH 4096
//////////////////////////////////////////////////////////////////////////
// Callbacks into the core
//////////////////////////////////////////////////////////////////////////

bool nscapi::core_wrapper::should_log(NSCAPI::nagiosReturn msgType) const {
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
void nscapi::core_wrapper::log(std::string message) const {
	if (!fNSAPIMessage) {
		return;
	}
	try {
		return fNSAPIMessage(message.c_str(), message.size());
	} catch (...) {
	}
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
void nscapi::core_wrapper::log(NSCAPI::nagiosReturn msgType, std::string file, int line, std::string logMessage) const {
	if (!should_log(msgType))
		return;
	if (!fNSAPISimpleMessage) {
		return;
	}
	try {
		return fNSAPISimpleMessage(alias.c_str(), msgType, file.c_str(), line, logMessage.c_str());
	} catch (...) {
	}
}

NSCAPI::log_level::level nscapi::core_wrapper::get_loglevel() const {
	if (!fNSAPIGetLoglevel) {
		return NSCAPI::log_level::debug;
	}
	return fNSAPIGetLoglevel();
}

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
NSCAPI::nagiosReturn nscapi::core_wrapper::query(const char *request, const unsigned int request_len, char **response, unsigned int *response_len) const {
	if (!fNSAPIInject)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	return fNSAPIInject(request, request_len, response, response_len);
}

void nscapi::core_wrapper::DestroyBuffer(char**buffer) const {
	if (!fNSAPIDestroyBuffer)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	return fNSAPIDestroyBuffer(buffer);
}

NSCAPI::errorReturn nscapi::core_wrapper::submit_message(std::string channel, std::string request, std::string &response) {
	if (!fNSAPINotify)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	char *buffer = NULL;
	unsigned int buffer_size = 0;
	NSCAPI::nagiosReturn ret = submit_message(channel.c_str(), request.c_str(), static_cast<unsigned int>(request.size()), &buffer, &buffer_size);

	if (buffer_size > 0 && buffer != NULL) {
		response = std::string(buffer, buffer_size);
	}

	DestroyBuffer(&buffer);
	return ret;
}

NSCAPI::errorReturn nscapi::core_wrapper::reload(std::string module) const {
	if (!fNSAPIReload)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	return fNSAPIReload(module.c_str());
}
NSCAPI::nagiosReturn nscapi::core_wrapper::submit_message(const char* channel, const char *request, const unsigned int request_len, char **response, unsigned int *response_len)
{
	if (!fNSAPINotify)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	return fNSAPINotify(channel, request, request_len, response, response_len);
}

NSCAPI::nagiosReturn nscapi::core_wrapper::query(const std::string & request, std::string & result) const {
	if (!fNSAPIInject)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	char *buffer = NULL;
	unsigned int buffer_size = 0;
	NSCAPI::nagiosReturn retC = query(request.c_str(), static_cast<unsigned int>(request.size()), &buffer, &buffer_size);

	if (buffer_size > 0 && buffer != NULL) {
		//PluginCommand::ResponseMessage rsp_msg;
		result = std::string(buffer, buffer_size);
	}

	DestroyBuffer(&buffer);
	if (retC != NSCAPI::isSuccess) {
		CORE_LOG_ERROR("Failed to execute command");
	}
	return retC;
}

NSCAPI::nagiosReturn nscapi::core_wrapper::exec_command(const std::string target, std::string request, std::string & result) {
	char *buffer = NULL;
	unsigned int buffer_size = 0;
	NSCAPI::nagiosReturn retC = exec_command(target.c_str(), request.c_str(), static_cast<unsigned int>(request.size()), &buffer, &buffer_size);

	if (buffer_size > 0 && buffer != NULL) {
		result = std::string(buffer, buffer_size);
	}

	DestroyBuffer(&buffer);
	if (retC != NSCAPI::isSuccess) {
		CORE_LOG_ERROR("Failed to execute command on " + target + ": " + strEx::s::xtos(retC));
	}
	return retC;
}
NSCAPI::nagiosReturn nscapi::core_wrapper::exec_command(const char* target, const char *request, const unsigned int request_len, char **response, unsigned int *response_len)
{
	if (!fNSAPIExecCommand)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	return fNSAPIExecCommand(target, request, request_len, response, response_len);
}

std::string nscapi::core_wrapper::expand_path(std::string value) {
	if (!fNSAPIExpandPath)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	unsigned int buf_len = LEGACY_BUFFER_LENGTH;
	char *buffer = new char[buf_len+1];
	if (fNSAPIExpandPath(value.c_str(), buffer, buf_len) != NSCAPI::isSuccess) {
		delete [] buffer;
		throw nscapi::nscapi_exception("Settings could not be retrieved.");
	}
	std::string ret = buffer;
	delete [] buffer;
	return ret;
}
NSCAPI::errorReturn nscapi::core_wrapper::settings_query(const char *request, const unsigned int request_len, char **response, unsigned int *response_len) const {
	if (!fNSAPISettingsQuery)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	return fNSAPISettingsQuery(request, request_len, response, response_len);
}
bool nscapi::core_wrapper::settings_query(const std::string request, std::string &response) const {
	char *buffer = NULL;
	unsigned int buffer_size = 0;
	NSCAPI::errorReturn retC = settings_query(request.c_str(), static_cast<unsigned int>(request.size()), &buffer, &buffer_size);
	if (buffer_size > 0 && buffer != NULL) {
		response = std::string(buffer, buffer_size);
	}
	DestroyBuffer(&buffer);
	return retC == NSCAPI::isSuccess;
}

NSCAPI::errorReturn nscapi::core_wrapper::registry_query(const char *request, const unsigned int request_len, char **response, unsigned int *response_len) const {
	if (!fNSAPIRegistryQuery)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	return fNSAPIRegistryQuery(request, request_len, response, response_len);
}
NSCAPI::errorReturn nscapi::core_wrapper::registry_query(const std::string request, std::string &response) const {
	char *buffer = NULL;
	unsigned int buffer_size = 0;
	NSCAPI::errorReturn retC = registry_query(request.c_str(), static_cast<unsigned int>(request.size()), &buffer, &buffer_size);
	if (buffer_size > 0 && buffer != NULL) {
		response = std::string(buffer, buffer_size);
	}
	DestroyBuffer(&buffer);
	return retC;
}

bool nscapi::core_wrapper::json_to_protobuf(const std::string &request, std::string &response) const {
	char *buffer = NULL;
	unsigned int buffer_size = 0;
	NSCAPI::errorReturn retC = json_to_protobuf(request.c_str(), static_cast<unsigned int>(request.size()), &buffer, &buffer_size);
	if (buffer_size > 0 && buffer != NULL) {
		response = std::string(buffer, buffer_size);
	}
	DestroyBuffer(&buffer);
	return retC == NSCAPI::isSuccess;
}

NSCAPI::errorReturn nscapi::core_wrapper::protobuf_to_json(const char *object, const char *request, const unsigned int request_len, char **response, unsigned int *response_len) const {
	if (!fNSCAPIProtobuf2Json)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	return fNSCAPIProtobuf2Json(object, request, request_len, response, response_len);
}

bool nscapi::core_wrapper::protobuf_to_json(const std::string &object, const std::string &request, std::string &response) const {
	char *buffer = NULL;
	unsigned int buffer_size = 0;
	NSCAPI::errorReturn retC = protobuf_to_json(object.c_str(), request.c_str(), static_cast<unsigned int>(request.size()), &buffer, &buffer_size);
	if (buffer_size > 0 && buffer != NULL) {
		response = std::string(buffer, buffer_size);
	}
	DestroyBuffer(&buffer);
	return retC == NSCAPI::isSuccess;
}

NSCAPI::errorReturn nscapi::core_wrapper::json_to_protobuf(const char *request, const unsigned int request_len, char **response, unsigned int *response_len) const {
	if (!fNSCAPIJson2Protobuf)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	return fNSCAPIJson2Protobuf(request, request_len, response, response_len);
}

/**
* Retrieve the application name (in human readable format) from the core.
* @return A string representing the application name.
* @throws nscapi::nscapi_exception When core pointer set is unavailable or an unexpected error occurs.
*/
std::string nscapi::core_wrapper::getApplicationName() {
	if (!fNSAPIGetApplicationName)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	unsigned int buf_len = LEGACY_BUFFER_LENGTH;
	char *buffer = new char[buf_len+1];
	if (fNSAPIGetApplicationName(buffer, buf_len) != NSCAPI::isSuccess) {
		delete [] buffer;
		throw nscapi::nscapi_exception("Application name could not be retrieved");
	}
	std::string ret = buffer;
	delete [] buffer;
	return ret;
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
std::string nscapi::core_wrapper::getApplicationVersionString() {
	if (!fNSAPIGetApplicationVersionStr)
		throw nscapi::nscapi_exception("NSCore has not been initiated...");
	unsigned int buf_len = LEGACY_BUFFER_LENGTH;
	char *buffer = new char[buf_len+1];
	if (fNSAPIGetApplicationVersionStr(buffer, buf_len) != NSCAPI::isSuccess) {
		delete [] buffer;
		return "";
	}
	std::string ret = buffer;
	delete [] buffer;
	return ret;
}

void nscapi::core_wrapper::set_alias(const std::string default_alias_, const std::string alias_) {
	alias = default_alias_;
}

/**
* Wrapper function around the ModuleHelperInit call.
* This wrapper retrieves all pointers and stores them for future use.
* @param f A function pointer to a function that can be used to load function from the core.
* @return NSCAPI::success or NSCAPI::failure
*/
bool nscapi::core_wrapper::load_endpoints(nscapi::core_api::lpNSAPILoader f) {
	fNSAPIGetApplicationName = (nscapi::core_api::lpNSAPIGetApplicationName)f("NSAPIGetApplicationName");
	fNSAPIGetApplicationVersionStr = (nscapi::core_api::lpNSAPIGetApplicationVersionStr)f("NSAPIGetApplicationVersionStr");
	fNSAPIMessage = (nscapi::core_api::lpNSAPIMessage)f("NSAPIMessage");
	fNSAPISimpleMessage = (nscapi::core_api::lpNSAPISimpleMessage)f("NSAPISimpleMessage");
	fNSAPIInject = (nscapi::core_api::lpNSAPIInject)f("NSAPIInject");
	fNSAPIExecCommand = (nscapi::core_api::lpNSAPIExecCommand)f("NSAPIExecCommand");
	fNSAPIDestroyBuffer = (nscapi::core_api::lpNSAPIDestroyBuffer)f("NSAPIDestroyBuffer");
	fNSAPINotify = (nscapi::core_api::lpNSAPINotify)f("NSAPINotify");
	fNSAPICheckLogMessages = (nscapi::core_api::lpNSAPICheckLogMessages)f("NSAPICheckLogMessages");
	fNSAPIReload = (nscapi::core_api::lpNSAPIReload)f("NSAPIReload");

	fNSAPISettingsQuery = (nscapi::core_api::lpNSAPISettingsQuery)f("NSAPISettingsQuery");
	fNSAPIRegistryQuery = (nscapi::core_api::lpNSAPIRegistryQuery)f("NSAPIRegistryQuery");
	fNSAPIExpandPath = (nscapi::core_api::lpNSAPIExpandPath)f("NSAPIExpandPath");

	fNSAPIGetLoglevel = (nscapi::core_api::lpNSAPIGetLoglevel)f("NSAPIGetLoglevel");

	fNSCAPIJson2Protobuf = (nscapi::core_api::lpNSCAPIJson2Protobuf)f("NSCAPIJson2Protobuf");
	fNSCAPIProtobuf2Json = (nscapi::core_api::lpNSCAPIProtobuf2Json)f("NSCAPIProtobuf2Json");

	return true;
}
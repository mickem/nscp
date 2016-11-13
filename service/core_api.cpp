/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "NSClient++.h"

#include <config.h>
#include "core_api.h"
#include <charEx.h>
#include <string.h>
#include <settings/settings_core.hpp>
#include <nscapi/nscapi_helper.hpp>
#ifdef _WIN32
#include <ServiceCmd.h>
#endif
#include <nsclient/logger/logger.hpp>

#ifdef HAVE_JSON_SPIRIT
//#define JSON_SPIRIT_VALUE_ENABLED
#include <json_spirit.h>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <plugin.pb-json.h>
#endif

#define LOG_ERROR_STD(msg) LOG_ERROR(((std::string)msg).c_str())
#define LOG_CRITICAL_STD(msg) LOG_CRITICAL(((std::string)msg).c_str())
#define LOG_MESSAGE_STD(msg) LOG_MESSAGE(((std::string)msg).c_str())
#define LOG_DEBUG_STD(msg) LOG_DEBUG(((std::string)msg).c_str())

#define LOG_ERROR(msg) { nsclient::logging::logger::get_logger()->error("core", __FILE__, __LINE__, msg); }
#define LOG_CRITICAL(msg) { nsclient::logging::logger::get_logger()->error("core", __FILE__, __LINE__, msg); }
#define LOG_MESSAGE(msg) { nsclient::logging::logger::get_logger()->info("core", __FILE__, __LINE__, msg); }
#define LOG_DEBUG(msg) { nsclient::logging::logger::get_logger()->debug("core", __FILE__, __LINE__, msg); }

extern NSClient *mainClient;	// Global core instance forward declaration.

NSCAPI::errorReturn NSAPIExpandPath(const char* key, char* buffer, unsigned int bufLen) {
	return nscapi::plugin_helper::wrapReturnString(buffer, bufLen, mainClient->expand_path(key), NSCAPI::api_return_codes::isSuccess);
}

NSCAPI::errorReturn NSAPIGetApplicationName(char *buffer, unsigned int bufLen) {
	return nscapi::plugin_helper::wrapReturnString(buffer, bufLen, utf8::cvt<std::string>(APPLICATION_NAME), NSCAPI::api_return_codes::isSuccess);
}
NSCAPI::errorReturn NSAPIGetApplicationVersionStr(char *buffer, unsigned int bufLen) {
	return nscapi::plugin_helper::wrapReturnString(buffer, bufLen, utf8::cvt<std::string>(CURRENT_SERVICE_VERSION), NSCAPI::api_return_codes::isSuccess);
}
void NSAPISimpleMessage(const char* module, int loglevel, const char* file, int line, const char* message) {
	if (loglevel == NSCAPI::log_level::critical) {
		mainClient->get_logger()->critical(module, file, line, message);
	} else if (loglevel == NSCAPI::log_level::error) {
		mainClient->get_logger()->error(module, file, line, message);
	} else if (loglevel == NSCAPI::log_level::warning) {
		mainClient->get_logger()->warning(module, file, line, message);
	} else if (loglevel == NSCAPI::log_level::info) {
		mainClient->get_logger()->info(module, file, line, message);
	} else if (loglevel == NSCAPI::log_level::debug) {
		mainClient->get_logger()->debug(module, file, line, message);
	} else if (loglevel == NSCAPI::log_level::trace) {
		mainClient->get_logger()->trace(module, file, line, message);
	} else {
		mainClient->get_logger()->critical(module, file, line, "Invalid log level for: " + std::string(message));
	}
}
void NSAPIMessage(const char* data, unsigned int count) {
	std::string message(data, count);
	mainClient->get_logger()->raw(message);
}
void NSAPIStopServer(void) {
	mainClient->get_service_control().stop();
}
NSCAPI::nagiosReturn NSAPIInject(const char *request_buffer, const unsigned int request_buffer_len, char **response_buffer, unsigned int *response_buffer_len) {
	std::string request(request_buffer, request_buffer_len), response;
	NSCAPI::nagiosReturn ret = mainClient->execute_query(request, response);
	*response_buffer_len = static_cast<unsigned int>(response.size());
	if (response.empty())
		*response_buffer = NULL;
	else {
		*response_buffer = new char[*response_buffer_len + 10];
		memcpy(*response_buffer, response.c_str(), *response_buffer_len);
	}
	return ret;
}

NSCAPI::nagiosReturn NSAPIExecCommand(const char* target, const char *request_buffer, const unsigned int request_buffer_len, char **response_buffer, unsigned int *response_buffer_len) {
	std::string request(request_buffer, request_buffer_len), response;
	NSCAPI::nagiosReturn ret = mainClient->exec_command(target, request, response);
	*response_buffer_len = static_cast<unsigned int>(response.size());
	if (response.empty())
		*response_buffer = NULL;
	else {
		*response_buffer = new char[*response_buffer_len + 10];
		memcpy(*response_buffer, response.c_str(), *response_buffer_len);
	}
	return ret;
}

NSCAPI::boolReturn NSAPICheckLogMessages(int loglevel) {
	if (loglevel == NSCAPI::log_level::critical) {
		return mainClient->get_logger()->should_critical();
	} else if (loglevel == NSCAPI::log_level::error) {
		return mainClient->get_logger()->should_error();
	} else if (loglevel == NSCAPI::log_level::warning) {
		return mainClient->get_logger()->should_warning();
	} else if (loglevel == NSCAPI::log_level::info) {
		return mainClient->get_logger()->should_info();
	} else if (loglevel == NSCAPI::log_level::debug) {
		return mainClient->get_logger()->should_debug();
	} else if (loglevel == NSCAPI::log_level::trace) {
		return mainClient->get_logger()->should_trace();
	} else {
		return true;
	}

}

NSCAPI::errorReturn NSAPISettingsQuery(const char *request_buffer, const unsigned int request_buffer_len, char **response_buffer, unsigned int *response_buffer_len) {
	return mainClient->settings_query(request_buffer, request_buffer_len, response_buffer, response_buffer_len);
}
NSCAPI::errorReturn NSAPIRegistryQuery(const char *request_buffer, const unsigned int request_buffer_len, char **response_buffer, unsigned int *response_buffer_len) {
	return mainClient->registry_query(request_buffer, request_buffer_len, response_buffer, response_buffer_len);
}

wchar_t* copyString(const std::wstring &str) {
	std::size_t sz = str.size();
	wchar_t *tc = new wchar_t[sz + 2];
	wcsncpy(tc, str.c_str(), sz);
	return tc;
}

NSCAPI::errorReturn NSAPIReload(const char *module) {
	return mainClient->reload(module);
}

nscapi::core_api::FUNPTR NSAPILoader(const char* buffer) {
	if (strcmp(buffer, "NSAPIGetApplicationName") == 0)
		return reinterpret_cast<nscapi::core_api::FUNPTR>(&NSAPIGetApplicationName);
	if (strcmp(buffer, "NSAPIGetApplicationVersionStr") == 0)
		return reinterpret_cast<nscapi::core_api::FUNPTR>(&NSAPIGetApplicationVersionStr);
	if (strcmp(buffer, "NSAPIMessage") == 0)
		return reinterpret_cast<nscapi::core_api::FUNPTR>(&NSAPIMessage);
	if (strcmp(buffer, "NSAPISimpleMessage") == 0)
		return reinterpret_cast<nscapi::core_api::FUNPTR>(&NSAPISimpleMessage);
	if (strcmp(buffer, "NSAPIInject") == 0)
		return reinterpret_cast<nscapi::core_api::FUNPTR>(&NSAPIInject);
	if (strcmp(buffer, "NSAPIExecCommand") == 0)
		return reinterpret_cast<nscapi::core_api::FUNPTR>(&NSAPIExecCommand);
	if (strcmp(buffer, "NSAPICheckLogMessages") == 0)
		return reinterpret_cast<nscapi::core_api::FUNPTR>(&NSAPICheckLogMessages);
	if (strcmp(buffer, "NSAPINotify") == 0)
		return reinterpret_cast<nscapi::core_api::FUNPTR>(&NSAPINotify);
	if (strcmp(buffer, "NSAPIDestroyBuffer") == 0)
		return reinterpret_cast<nscapi::core_api::FUNPTR>(&NSAPIDestroyBuffer);
	if (strcmp(buffer, "NSAPIExpandPath") == 0)
		return reinterpret_cast<nscapi::core_api::FUNPTR>(&NSAPIExpandPath);
	if (strcmp(buffer, "NSAPIReload") == 0)
		return reinterpret_cast<nscapi::core_api::FUNPTR>(&NSAPIReload);
	if (strcmp(buffer, "NSAPIGetLoglevel") == 0)
		return reinterpret_cast<nscapi::core_api::FUNPTR>(&NSAPIGetLoglevel);
	if (strcmp(buffer, "NSAPISettingsQuery") == 0)
		return reinterpret_cast<nscapi::core_api::FUNPTR>(&NSAPISettingsQuery);
	if (strcmp(buffer, "NSAPIRegistryQuery") == 0)
		return reinterpret_cast<nscapi::core_api::FUNPTR>(&NSAPIRegistryQuery);
	if (strcmp(buffer, "NSCAPIJson2Protobuf") == 0)
		return reinterpret_cast<nscapi::core_api::FUNPTR>(&NSCAPIJson2Protobuf);
	if (strcmp(buffer, "NSCAPIProtobuf2Json") == 0)
		return reinterpret_cast<nscapi::core_api::FUNPTR>(&NSCAPIProtobuf2Json);
	if (strcmp(buffer, "NSCAPIEmitEvent") == 0)
		return reinterpret_cast<nscapi::core_api::FUNPTR>(&NSCAPIEmitEvent);
	mainClient->get_logger()->critical("api", __FILE__, __LINE__, "Function not found: " + std::string(buffer));
	return NULL;
}

NSCAPI::errorReturn NSAPINotify(const char* channel, const char* request_buffer, unsigned int request_buffer_len, char ** response_buffer, unsigned int *response_buffer_len) {
	std::string request(request_buffer, request_buffer_len), response;
	NSCAPI::nagiosReturn ret = mainClient->send_notification(channel, request, response);
	*response_buffer_len = static_cast<unsigned int>(response.size());
	if (response.empty())
		*response_buffer = NULL;
	else {
		*response_buffer = new char[*response_buffer_len + 10];
		memcpy(*response_buffer, response.c_str(), *response_buffer_len);
	}
	return ret;
}

void NSAPIDestroyBuffer(char**buffer) {
	delete[] * buffer;
}

NSCAPI::log_level::level NSAPIGetLoglevel() {
	std::string log = mainClient->get_logger()->get_log_level();
	if (log == "critical")
		return NSCAPI::log_level::critical;
	if (log == "error")
		return NSCAPI::log_level::error;
	if (log == "warning")
		return NSCAPI::log_level::warning;
	if (log == "info")
		return NSCAPI::log_level::info;
	if (log == "debug")
		return NSCAPI::log_level::debug;
	if (log == "trace")
		return NSCAPI::log_level::trace;
	return NSCAPI::log_level::unknown;
}

#ifdef HAVE_JSON_SPIRIT
#include <nscapi/nscapi_protobuf.hpp>

NSCAPI::errorReturn NSCAPIJson2Protobuf(const char* request_buffer, unsigned int request_buffer_len, char ** response_buffer, unsigned int *response_buffer_len) {
	std::string request(request_buffer, request_buffer_len), response;
	try {
		json_spirit::Value root;
		json_spirit::read_or_throw(request, root);
		std::string object_type;
		json_spirit::Object o = root.getObject();
		BOOST_FOREACH(const json_spirit::Object::value_type &p, o) {
			if (p.first == "type" && p.second.type() == json_spirit::Value::STRING_TYPE)
				object_type = p.second.getString();
		}
		std::string response;
		if (object_type.empty()) {
			mainClient->get_logger()->error("api", __FILE__, __LINE__, "Missing type or payload.");
			return NSCAPI::api_return_codes::hasFailed;
		} else if (object_type == "SettingsRequestMessage") {
			Plugin::SettingsRequestMessage request_message;
			json_pb::Plugin::SettingsRequestMessage::to_pb(&request_message, o);
			response = request_message.SerializeAsString();
		} else if (object_type == "RegistryRequestMessage") {
			Plugin::RegistryRequestMessage request_message;
			json_pb::Plugin::RegistryRequestMessage::to_pb(&request_message, o);
			response = request_message.SerializeAsString();
		} else {
			mainClient->get_logger()->error("api", __FILE__, __LINE__, "Missing type or payload.");
			return NSCAPI::api_return_codes::hasFailed;
		}
		*response_buffer_len = static_cast<unsigned int>(response.size());
		if (response.empty())
			*response_buffer = NULL;
		else {
			*response_buffer = new char[*response_buffer_len + 10];
			memcpy(*response_buffer, response.c_str(), *response_buffer_len);
		}
	} catch (const json_spirit::ParseError &e) {
		mainClient->get_logger()->error("api", __FILE__, __LINE__, "Failed to parse JSON: " + e.reason_);
		return NSCAPI::api_return_codes::hasFailed;
	}
	return NSCAPI::api_return_codes::isSuccess;
}

NSCAPI::errorReturn NSCAPIProtobuf2Json(const char* object, const char* request_buffer, unsigned int request_buffer_len, char ** response_buffer, unsigned int *response_buffer_len) {
	std::string request(request_buffer, request_buffer_len), response, obj(object);
	try {
		json_spirit::Object root;
		if (obj == "SettingsResponseMessage") {
			Plugin::SettingsResponseMessage message;
			message.ParseFromString(request);
			root = json_pb::Plugin::SettingsResponseMessage::to_json(message);
		} else if (obj == "RegistryResponseMessage") {
			Plugin::RegistryResponseMessage message;
			message.ParseFromString(request);
			root = json_pb::Plugin::RegistryResponseMessage::to_json(message);
		} else if (obj == "QueryResponseMessage") {
			Plugin::QueryResponseMessage message;
			message.ParseFromString(request);
			root = json_pb::Plugin::QueryResponseMessage::to_json(message);
		} else if (obj == "ExecuteResponseMessage") {
			Plugin::ExecuteResponseMessage message;
			message.ParseFromString(request);
			root = json_pb::Plugin::ExecuteResponseMessage::to_json(message);
		} else {
			mainClient->get_logger()->error("api", __FILE__, __LINE__, "Invalid type: " + obj);
			return NSCAPI::api_return_codes::hasFailed;
		}
		std::string response = json_spirit::write(root);
		*response_buffer_len = static_cast<unsigned int>(response.size());
		if (response.empty())
			*response_buffer = NULL;
		else {
			*response_buffer = new char[*response_buffer_len + 10];
			memcpy(*response_buffer, response.c_str(), *response_buffer_len);
		}
	} catch (const json_spirit::ParseError &e) {
		mainClient->get_logger()->error("api", __FILE__, __LINE__, "Failed to parse JSON: " + e.reason_);
		return NSCAPI::api_return_codes::hasFailed;
	}
	return NSCAPI::api_return_codes::isSuccess;
}
#else
NSCAPI::errorReturn NSCAPIJson2protobuf(const char* request_buffer, unsigned int request_buffer_len, char ** response_buffer, unsigned int *response_buffer_len) {
	mainClient->get_logger()->error("api", __FILE__, __LINE__, "Not compiled with jason spirit so json not supported");
	return NSCAPI::hasFailed;
}
NSCAPI::errorReturn NSCAPIProtobuf2Json(const char* object, const char* request_buffer, unsigned int request_buffer_len, char ** response_buffer, unsigned int *response_buffer_len) {
	mainClient->get_logger()->error("api", __FILE__, __LINE__, "Not compiled with jason spirit so json not supported");
	return NSCAPI::hasFailed;
}
#endif


NSCAPI::errorReturn NSCAPIEmitEvent(const char* request_buffer, unsigned int request_buffer_len) {
	std::string request(request_buffer, request_buffer_len);
	return mainClient->emit_event(request);
}

/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "NSClient++.h"

#include <config.h>
#include "core_api.h"
#include <string.h>
#include <settings/settings_core.hpp>
#include <nscapi/nscapi_helper.hpp>
#ifdef _WIN32
#include <ServiceCmd.h>
#endif
#include <nsclient/logger/logger.hpp>

#ifdef HAVE_JSON_SPIRIT
#include <json_spirit.h>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <plugin.pb-json.h>
#endif

#include "settings_query_handler.hpp"
#include "registry_query_handler.hpp"
#include "storage_query_handler.hpp"

#define LOG_ERROR(core, msg) { core->get_logger()->error("core", __FILE__, __LINE__, msg); }

extern NSClient *mainClient;	// Global core instance forward declaration.

NSCAPI::errorReturn NSAPIExpandPath(const char* key, char* buffer, unsigned int bufLen) {
	return nscapi::plugin_helper::wrapReturnString(buffer, bufLen, mainClient->get_path()->expand_path(key), NSCAPI::api_return_codes::isSuccess);
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
	NSCAPI::nagiosReturn ret = mainClient->get_plugin_manager()->execute_query(request, response);
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
	NSCAPI::nagiosReturn ret = mainClient->get_plugin_manager()->exec_command(target, request, response);
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

	try {
		Plugin::SettingsRequestMessage request;
		Plugin::SettingsResponseMessage response;
		request.ParseFromArray(request_buffer, request_buffer_len);

		nsclient::core::settings_query_handler sqr(mainClient, request);
		sqr.parse(response);
		*response_buffer_len = response.ByteSize();
		*response_buffer = new char[*response_buffer_len + 10];
		response.SerializeToArray(*response_buffer, *response_buffer_len);
		return NSCAPI::api_return_codes::isSuccess;
	} catch (const std::exception &e) {
		LOG_ERROR(mainClient, "Settings query error: " + utf8::utf8_from_native(e.what()));
	} catch (...) {
		LOG_ERROR(mainClient, "Unknown settings query error");
	}
	return NSCAPI::api_return_codes::hasFailed;
}
NSCAPI::errorReturn NSAPIRegistryQuery(const char *request_buffer, const unsigned int request_buffer_len, char **response_buffer, unsigned int *response_buffer_len) {
	try {
		std::string response_string;
		Plugin::RegistryRequestMessage request;
		Plugin::RegistryResponseMessage response;
		request.ParseFromArray(request_buffer, request_buffer_len);
		nsclient::core::registry_query_handler rqh(mainClient->get_path(), mainClient->get_plugin_manager(), mainClient->get_logger(), request);
		rqh.parse(response);

		*response_buffer_len = response.ByteSize();
		*response_buffer = new char[*response_buffer_len + 10];
		response.SerializeToArray(*response_buffer, *response_buffer_len);
	} catch (settings::settings_exception e) {
		LOG_ERROR(mainClient, "Failed query: " + e.reason());
		return NSCAPI::api_return_codes::hasFailed;
	} catch (const std::exception &e) {
		LOG_ERROR(mainClient, "Failed query: " + utf8::utf8_from_native(e.what()));
		return NSCAPI::api_return_codes::hasFailed;
	} catch (...) {
		LOG_ERROR(mainClient, "Failed query");
		return NSCAPI::api_return_codes::hasFailed;
	}
	return NSCAPI::api_return_codes::isSuccess;
}

NSCAPI::errorReturn NSCAPIStorageQuery(const char *request_buffer, const unsigned int request_buffer_len, char **response_buffer, unsigned int *response_buffer_len) {

	try {
		Plugin::StorageRequestMessage request;
		Plugin::StorageResponseMessage response;
		request.ParseFromArray(request_buffer, request_buffer_len);

		nsclient::core::storage_query_handler sqr(mainClient->get_storage_manager(), mainClient->get_plugin_manager(), mainClient->get_logger(), request);
		sqr.parse(response);
		*response_buffer_len = response.ByteSize();
		*response_buffer = new char[*response_buffer_len + 10];
		response.SerializeToArray(*response_buffer, *response_buffer_len);
		return NSCAPI::api_return_codes::isSuccess;
	} catch (const std::exception &e) {
		LOG_ERROR(mainClient, "Storage query error: " + utf8::utf8_from_native(e.what()));
	} catch (...) {
		LOG_ERROR(mainClient, "Unknown settings query error");
	}
	return NSCAPI::api_return_codes::hasFailed;
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
	if (strcmp(buffer, "NSAPIStorageQuery") == 0)
		return reinterpret_cast<nscapi::core_api::FUNPTR>(&NSCAPIStorageQuery);
	mainClient->get_logger()->critical("api", __FILE__, __LINE__, "Function not found: " + std::string(buffer));
	return NULL;
}

NSCAPI::errorReturn NSAPINotify(const char* channel, const char* request_buffer, unsigned int request_buffer_len, char ** response_buffer, unsigned int *response_buffer_len) {
	std::string request(request_buffer, request_buffer_len), response;
	NSCAPI::nagiosReturn ret = mainClient->get_plugin_manager()->send_notification(channel, request, response);
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
NSCAPI::errorReturn NSCAPIJson2Protobuf(const char* request_buffer, unsigned int request_buffer_len, char ** response_buffer, unsigned int *response_buffer_len) {
	mainClient->get_logger()->error("api", __FILE__, __LINE__, "Not compiled with json spirit so json not supported");
	return NSCAPI::api_return_codes::hasFailed;
}
NSCAPI::errorReturn NSCAPIProtobuf2Json(const char* object, const char* request_buffer, unsigned int request_buffer_len, char ** response_buffer, unsigned int *response_buffer_len) {
	mainClient->get_logger()->error("api", __FILE__, __LINE__, "Not compiled with json spirit so json not supported");
	return NSCAPI::api_return_codes::hasFailed;
}
#endif


NSCAPI::errorReturn NSCAPIEmitEvent(const char* request_buffer, unsigned int request_buffer_len) {
	std::string request(request_buffer, request_buffer_len);
	return mainClient->get_plugin_manager()->emit_event(request);
}

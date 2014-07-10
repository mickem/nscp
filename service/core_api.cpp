///////////////////////////////////////////////////////////////////////////
// NSClient++ Base Service
// 
// Copyright (c) 2004 MySolutions NORDIC (http://www.medin.name)
//
// Date: 2004-03-13
// Author: Michael Medin (michael@medin.name)
//
// Part of this file is based on work by Bruno Vais (bvais@usa.net)
//
// This software is provided "AS IS", without a warranty of any kind.
// You are free to use/modify this code but leave this header intact.
//
//////////////////////////////////////////////////////////////////////////
#include "NSClient++.h"

#include <config.h>
#include "core_api.h"
#include <charEx.h>
#include <string.h>
#include <settings/settings_core.hpp>
#include "../helpers/settings_manager/settings_manager_impl.h"
#include <nscapi/nscapi_helper.hpp>
#ifdef _WIN32
#include <ServiceCmd.h>
#endif
#include <nsclient/logger.hpp>

#ifdef HAVE_JSON_SPIRIT
//#define JSON_SPIRIT_VALUE_ENABLED
#include <json_spirit.h>
#include <nscapi/nscapi_protobuf_functions.hpp>
#endif

#define LOG_ERROR_STD(msg) LOG_ERROR(((std::string)msg).c_str())
#define LOG_CRITICAL_STD(msg) LOG_CRITICAL(((std::string)msg).c_str())
#define LOG_MESSAGE_STD(msg) LOG_MESSAGE(((std::string)msg).c_str())
#define LOG_DEBUG_STD(msg) LOG_DEBUG(((std::string)msg).c_str())

#define LOG_ERROR(msg) { nsclient::logging::logger::get_logger()->error("core", __FILE__, __LINE__, msg); }
#define LOG_CRITICAL(msg) { nsclient::logging::logger::get_logger()->error("core", __FILE__, __LINE__, msg); }
#define LOG_MESSAGE(msg) { nsclient::logging::logger::get_logger()->info("core", __FILE__, __LINE__, msg); }
#define LOG_DEBUG(msg) { nsclient::logging::logger::get_logger()->debug("core", __FILE__, __LINE__, msg); }

NSCAPI::errorReturn NSAPIExpandPath(const char* key, char* buffer,unsigned int bufLen) {
	try {
		return nscapi::plugin_helper::wrapReturnString(buffer, bufLen, mainClient.expand_path(key), NSCAPI::isSuccess);
	} catch (...) {
		LOG_ERROR_STD("Failed to getString: " + utf8::cvt<std::string>(key));
		return NSCAPI::hasFailed;
	}
}

NSCAPI::errorReturn NSAPIGetApplicationName(char *buffer, unsigned int bufLen) {
	return nscapi::plugin_helper::wrapReturnString(buffer, bufLen, utf8::cvt<std::string>(APPLICATION_NAME), NSCAPI::isSuccess);
}
NSCAPI::errorReturn NSAPIGetApplicationVersionStr(char *buffer, unsigned int bufLen) {
	return nscapi::plugin_helper::wrapReturnString(buffer, bufLen, utf8::cvt<std::string>(CURRENT_SERVICE_VERSION), NSCAPI::isSuccess);
}
void NSAPISimpleMessage(const char* module, int loglevel, const char* file, int line, const char* message) {
	nsclient::logging::logger::get_logger()->log(module, loglevel, file, line, message);
}
void NSAPIMessage(const char* data, unsigned int count) {
	std::string message(data, count);
	nsclient::logging::logger::get_logger()->raw(message);
}
void NSAPIStopServer(void) {
	mainClient.get_service_control().stop();
}
NSCAPI::nagiosReturn NSAPIInject(const char *request_buffer, const unsigned int request_buffer_len, char **response_buffer, unsigned int *response_buffer_len) {
	std::string request (request_buffer, request_buffer_len), response;
	NSCAPI::nagiosReturn ret = mainClient.injectRAW(request, response);
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
	std::string request (request_buffer, request_buffer_len), response;
	NSCAPI::nagiosReturn ret = mainClient.exec_command(target, request, response);
	*response_buffer_len = static_cast<unsigned int>(response.size());
	if (response.empty())
		*response_buffer = NULL;
	else {
		*response_buffer = new char[*response_buffer_len + 10];
		memcpy(*response_buffer, response.c_str(), *response_buffer_len);
	}
	return ret;
}


NSCAPI::boolReturn NSAPICheckLogMessages(int messageType) {
	return nsclient::logging::logger::get_logger()->should_log(messageType);
}

NSCAPI::errorReturn NSAPIEncrypt(unsigned int algorithm, const wchar_t*, unsigned int, wchar_t*, unsigned int*) {
	if (algorithm != NSCAPI::encryption_xor) {
		LOG_ERROR("Unknown algortihm requested.");
		return NSCAPI::hasFailed;
	}
	/*
	TODO reimplement this

	std::wstring key = settings_manager::get_settings()->get_string(SETTINGS_KEY(protocol_def::MASTER_KEY));
	int tcharInBufLen = 0;
	char *c = charEx::tchar_to_char(inBuffer, inBufLen, tcharInBufLen);
	std::wstring::size_type j=0;
	for (int i=0;i<tcharInBufLen;i++,j++) {
		if (j > key.size())
			j = 0;
		c[i] ^= key[j];
	}
	size_t cOutBufLen = b64::b64_encode(reinterpret_cast<void*>(c), tcharInBufLen, NULL, NULL);
	if (!outBuf) {
		*outBufLen = static_cast<unsigned int>(cOutBufLen*2); // TODO: Guessing wildly here but no proper way to tell without a lot of extra work
		return NSCAPI::isSuccess;
	}
	char *cOutBuf = new char[cOutBufLen+1];
	size_t len = b64::b64_encode(reinterpret_cast<void*>(c), tcharInBufLen, cOutBuf, cOutBufLen);
	delete [] c;
	if (len == 0) {
		LOG_ERROR(_T("Invalid out buffer length."));
		return NSCAPI::isInvalidBufferLen;
	}
	int realOutLen;
	wchar_t *realOut = charEx::char_to_tchar(cOutBuf, cOutBufLen, realOutLen);
	if (static_cast<unsigned int>(realOutLen) >= *outBufLen) {
		LOG_ERROR_STD(_T("Invalid out buffer length: ") + strEx::itos(realOutLen) + _T(" was needed but only ") + strEx::itos(*outBufLen) + _T(" was allocated."));
		return NSCAPI::isInvalidBufferLen;
	}
	wcsncpy(outBuf, *outBufLen, realOut);
	delete [] realOut;
	outBuf[realOutLen] = 0;
	*outBufLen = static_cast<unsigned int>(realOutLen);
	*/
	return NSCAPI::isSuccess;
}

NSCAPI::errorReturn NSAPIDecrypt(unsigned int algorithm, const wchar_t*, unsigned int, wchar_t*, unsigned int *) {
	if (algorithm != NSCAPI::encryption_xor) {
		LOG_ERROR("Unknown algortihm requested.");
		return NSCAPI::hasFailed;
	}
	/*
	int inBufLenC = 0;
	char *inBufferC = charEx::tchar_to_char(inBuffer, inBufLen, inBufLenC);
	size_t cOutLen =  b64::b64_decode(inBufferC, inBufLenC, NULL, NULL);
	if (!outBuf) {
		*outBufLen = static_cast<unsigned int>(cOutLen*2); // TODO: Guessing wildly here but no proper way to tell without a lot of extra work
		return NSCAPI::isSuccess;
	}
	char *cOutBuf = new char[cOutLen+1];
	size_t len = b64::b64_decode(inBufferC, inBufLenC, reinterpret_cast<void*>(cOutBuf), cOutLen);
	delete [] inBufferC;
	if (len == 0) {
		LOG_ERROR(_T("Invalid out buffer length."));
		return NSCAPI::isInvalidBufferLen;
	}
	int realOutLen;

	std::wstring key = settings_manager::get_settings()->get_string(SETTINGS_KEY(protocol_def::MASTER_KEY));
	std::wstring::size_type j=0;
	for (int i=0;i<cOutLen;i++,j++) {
		if (j > key.size())
			j = 0;
		cOutBuf[i] ^= key[j];
	}

	wchar_t *realOut = charEx::char_to_tchar(cOutBuf, cOutLen, realOutLen);
	if (static_cast<unsigned int>(realOutLen) >= *outBufLen) {
		LOG_ERROR_STD(_T("Invalid out buffer length: ") + strEx::itos(realOutLen) + _T(" was needed but only ") + strEx::itos(*outBufLen) + _T(" was allocated."));
		return NSCAPI::isInvalidBufferLen;
	}
	wcsncpy(outBuf, *outBufLen, realOut);
	delete [] realOut;
	outBuf[realOutLen] = 0;
	*outBufLen = static_cast<unsigned int>(realOutLen);
	*/
	return NSCAPI::isSuccess;
}

NSCAPI::errorReturn NSAPISettingsQuery(const char *request_buffer, const unsigned int request_buffer_len, char **response_buffer, unsigned int *response_buffer_len) {
	return mainClient.settings_query(request_buffer, request_buffer_len, response_buffer, response_buffer_len);
}
NSCAPI::errorReturn NSAPIRegistryQuery(const char *request_buffer, const unsigned int request_buffer_len, char **response_buffer, unsigned int *response_buffer_len) {
	return mainClient.registry_query(request_buffer, request_buffer_len, response_buffer, response_buffer_len);
}

wchar_t* copyString(const std::wstring &str) {
	std::size_t sz = str.size();
	wchar_t *tc = new wchar_t[sz+2];
	wcsncpy(tc, str.c_str(), sz);
	return tc;
}


NSCAPI::errorReturn NSAPIReload(const char *module) {
	try {
		return mainClient.reload(module);
	} catch (...) {
		LOG_ERROR_STD("Reload failed");
		return NSCAPI::hasFailed;
	}
}

void* NSAPILoader(const char* buffer) {
	if (strcmp(buffer, "NSAPIGetApplicationName") == 0)
		return reinterpret_cast<void*>(&NSAPIGetApplicationName);
	if (strcmp(buffer, "NSAPIGetApplicationVersionStr") == 0)
		return reinterpret_cast<void*>(&NSAPIGetApplicationVersionStr);
	if (strcmp(buffer, "NSAPIMessage") == 0)
		return reinterpret_cast<void*>(&NSAPIMessage);
	if (strcmp(buffer, "NSAPISimpleMessage") == 0)
		return reinterpret_cast<void*>(&NSAPISimpleMessage);
	if (strcmp(buffer, "NSAPIInject") == 0)
		return reinterpret_cast<void*>(&NSAPIInject);
	if (strcmp(buffer, "NSAPIExecCommand") == 0)
		return reinterpret_cast<void*>(&NSAPIExecCommand);
	if (strcmp(buffer, "NSAPICheckLogMessages") == 0)
		return reinterpret_cast<void*>(&NSAPICheckLogMessages);
	if (strcmp(buffer, "NSAPIEncrypt") == 0)
		return reinterpret_cast<void*>(&NSAPIEncrypt);
	if (strcmp(buffer, "NSAPIDecrypt") == 0)
		return reinterpret_cast<void*>(&NSAPIDecrypt);
	if (strcmp(buffer, "NSAPINotify") == 0)
		return reinterpret_cast<void*>(&NSAPINotify);
	if (strcmp(buffer, "NSAPIDestroyBuffer") == 0)
		return reinterpret_cast<void*>(&NSAPIDestroyBuffer);
	if (strcmp(buffer, "NSAPIExpandPath") == 0)
		return reinterpret_cast<void*>(&NSAPIExpandPath);
	if (strcmp(buffer, "NSAPIReload") == 0)
		return reinterpret_cast<void*>(&NSAPIReload);
	if (strcmp(buffer, "NSAPIGetLoglevel") == 0)
		return reinterpret_cast<void*>(&NSAPIGetLoglevel);
	if (strcmp(buffer, "NSAPISettingsQuery") == 0)
		return reinterpret_cast<void*>(&NSAPISettingsQuery);
	if (strcmp(buffer, "NSAPIRegistryQuery") == 0)
		return reinterpret_cast<void*>(&NSAPIRegistryQuery);
#ifdef HAVE_JSON_SPIRIT
	if (strcmp(buffer, "NSCAPIJson2Protobuf") == 0)
		return reinterpret_cast<void*>(&NSCAPIJson2Protobuf);
	if (strcmp(buffer, "NSCAPIProtobuf2Json") == 0)
		return reinterpret_cast<void*>(&NSCAPIProtobuf2Json);
#endif
	LOG_ERROR_STD("Function not found: " + buffer);
	return NULL;
}

NSCAPI::errorReturn NSAPINotify(const char* channel, const char* request_buffer, unsigned int request_buffer_len, char ** response_buffer, unsigned int *response_buffer_len) {
	std::string request (request_buffer, request_buffer_len), response;
	NSCAPI::nagiosReturn ret = mainClient.send_notification(channel, request, response);
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
	delete [] *buffer;
}

NSCAPI::log_level::level NSAPIGetLoglevel() {
	return nsclient::logging::logger::get_logger()->get_log_level();
}


#ifdef HAVE_JSON_SPIRIT
#include <nscapi/nscapi_protobuf.hpp>
/*
ROOT Level
{
	type: "SettingsRequestMessage"
	header: {...}
	payloads: [{...}, {...}]
}
{
	type: "SettingsRequestMessage"
	header: {...}
	payload: {...}
}
*/

#define SET_STR(obj, src, tag) if (src.second.isString() && src.first == # tag) { obj->set_ ## tag(src.second.getString()); continue; }
#define SET_STR_EX(obj, src, tag, target) if (src.second.isString() && src.first == # tag) { obj->set_ ## target(src.second.getString()); continue; }
#define SET_BOOL(obj, src, tag) if (src.second.isBool() && src.first == # tag) { obj->set_ ## tag(src.second.getBool()); continue; }
#define SET_STR_LIST(obj, src, tag) if (src.second.isString() && src.first == # tag) { obj->add_ ## tag(src.second.getString()); continue; } \
	if (src.second.isArray() && src.first == # tag) { BOOST_FOREACH(const json_spirit::Value &s, src.second.getArray()) { if (s.isString()) { obj->add_ ## tag(s.getString());}} continue; }
#define SET_INT64(obj, src, tag) if (src.second.isInt64() && src.first == # tag) { obj->set_ ## tag(src.second.getInt64()); continue; }
#define SET_INT(obj, src, tag) if (src.second.isInt() && src.first == # tag) { obj->set_ ## tag(src.second.getInt()); continue; }
#define DELEGATE_OBJ(obj, src, tag, fun) if (src.second.isObject() && src.first == # tag) { fun(obj->mutable_ ## tag(), src.second.getObject()); continue; }


void parse_json_header(Plugin::Common_Header* gpb, const json_spirit::Object &json) {
	BOOST_FOREACH(const json_spirit::Object::value_type &node, json) {
		if (node.second.type() == json_spirit::Value::STRING_TYPE) {
			if (node.first == "version") {
				if (node.second.getString() == "1")
					gpb->set_version(Plugin::Common_Version_VERSION_1);
				else
					LOG_ERROR_STD("Invalid version: " + node.second.getString());
			} else if (node.first == "max_supported_version") {
				if (node.second.getString() == "1")
					gpb->set_max_supported_version(Plugin::Common_Version_VERSION_1);
				else
					LOG_ERROR_STD("Invalid max_supported_version: " + node.second.getString());
			}
		}
		SET_STR(gpb, node, source_id);
		SET_STR(gpb, node, sender_id);
		SET_STR(gpb, node, recipient_id);
		SET_STR(gpb, node, destination_id);
		SET_INT64(gpb, node, message_id);
	}
	if (!gpb->has_version())
		gpb->set_version(Plugin::Common_Version_VERSION_1);

}

void parse_json_settings_common_node(Plugin::Settings::Node* gpb, const json_spirit::Object &json) {
	BOOST_FOREACH(const json_spirit::Object::value_type &node, json) {
		SET_STR(gpb, node, path);
		SET_STR(gpb, node, key);
	}
}

template<class T>
void parse_json_settings_common_type(T* gpb, const json_spirit::Object::value_type &node, const std::string &alias) {
	if (node.second.isString() && node.first == alias) {
		std::string key = boost::to_upper_copy(node.second.getString());
		if (key == "INT")
			gpb->set_type(Plugin::Common_DataType_INT);
		else if (key == "STRING")
			gpb->set_type(Plugin::Common_DataType_STRING);
		else if (key == "FLOAT")
			gpb->set_type(Plugin::Common_DataType_FLOAT);
		else if (key == "BOOL")
			gpb->set_type(Plugin::Common_DataType_BOOL);
		else if (key == "LIST")
			gpb->set_type(Plugin::Common_DataType_LIST);
	}
}

template<class T>
void parse_json_registry_common_type(T* gpb, const json_spirit::Object::value_type &node, const std::string &alias) {
	if (node.second.isString() && node.first == alias) {
		if (node.second == "QUERY")
			gpb->set_type(Plugin::Registry::QUERY);
		else if (node.second == "COMMAND")
			gpb->set_type(Plugin::Registry::COMMAND);
		else if (node.second == "HANDLER")
			gpb->set_type(Plugin::Registry::HANDLER);
		else if (node.second == "PLUGIN")
			gpb->set_type(Plugin::Registry::PLUGIN);
		else if (node.second == "QUERY_ALIAS")
			gpb->set_type(Plugin::Registry::QUERY_ALIAS);
		else if (node.second == "ROUTER")
			gpb->set_type(Plugin::Registry::ROUTER);
		else if (node.second == "ALL")
			gpb->set_type(Plugin::Registry::ALL);
	}
}

void parse_json_common_any_data(Plugin::Common::AnyDataType* gpb, const json_spirit::Object &json) {
	BOOST_FOREACH(const json_spirit::Object::value_type &node, json) {
		parse_json_settings_common_type(gpb, node, "type");
		SET_STR(gpb, node, string_data);
		SET_STR_EX(gpb, node, value, string_data);
		SET_INT64(gpb, node, int_data);
		//SET_FLOAT(gpb, node, float_data);
		SET_BOOL(gpb, node, bool_data);
		SET_STR(gpb, node, string_data);
		SET_STR_LIST(gpb, node, list_data);
	}
}

void parse_json_settings_common_info(Plugin::Settings::Information* gpb, const json_spirit::Object &json) {
	BOOST_FOREACH(const json_spirit::Object::value_type &node, json) {
		SET_STR(gpb, node, title);
		SET_STR(gpb, node, description);
		DELEGATE_OBJ(gpb, node, default_value, parse_json_common_any_data);
		SET_STR(gpb, node, min_version);
		SET_STR(gpb, node, max_version);
		SET_BOOL(gpb, node, advanced);
		SET_BOOL(gpb, node, sample);
		SET_STR(gpb, node, sample_usage);
		SET_STR_LIST(gpb, node, plugin);
	}
}

void parse_json_registry_common_info(Plugin::Registry::Information* gpb, const json_spirit::Object &json) {
	BOOST_FOREACH(const json_spirit::Object::value_type &node, json) {
		SET_STR(gpb, node, title);
		SET_STR(gpb, node, description);
		// TODO: metadata
		SET_STR(gpb, node, min_version);
		SET_STR(gpb, node, max_version);
		SET_BOOL(gpb, node, advanced);
		SET_STR_LIST(gpb, node, plugin);
	}
}

void parse_json_settings_payload_registration(Plugin::SettingsRequestMessage::Request::Registration* gpb, const json_spirit::Object &json) {
	BOOST_FOREACH(const json_spirit::Object::value_type &node, json) {
		DELEGATE_OBJ(gpb, node, node, parse_json_settings_common_node);
		DELEGATE_OBJ(gpb, node, info, parse_json_settings_common_info);
	}
}
void parse_json_registry_payload_registration(Plugin::RegistryRequestMessage::Request::Registration* gpb, const json_spirit::Object &json) {
	BOOST_FOREACH(const json_spirit::Object::value_type &node, json) {
		SET_INT(gpb, node, plugin_id);
		parse_json_registry_common_type(gpb, node, "type");
		SET_STR(gpb, node, name);
		DELEGATE_OBJ(gpb, node, info, parse_json_registry_common_info);
		SET_STR_LIST(gpb, node, alias);
	}
}
void parse_json_settings_payload_query(Plugin::SettingsRequestMessage::Request::Query* gpb, const json_spirit::Object &json) {
	BOOST_FOREACH(const json_spirit::Object::value_type &node, json) {
		DELEGATE_OBJ(gpb, node, node, parse_json_settings_common_node);
		// TODO:; QUery
		SET_BOOL(gpb, node, recursive);
		parse_json_settings_common_type(gpb, node, "type");
		DELEGATE_OBJ(gpb, node, default_value, parse_json_common_any_data);
	}
}
void parse_json_settings_payload_update(Plugin::SettingsRequestMessage::Request::Update* gpb, const json_spirit::Object &json) {
	BOOST_FOREACH(const json_spirit::Object::value_type &node, json) {
		DELEGATE_OBJ(gpb, node, node, parse_json_settings_common_node);
		DELEGATE_OBJ(gpb, node, value, parse_json_common_any_data);
	}
}
void parse_json_settings_payload_control(Plugin::SettingsRequestMessage::Request::Control* gpb, const json_spirit::Object &json) {
	/*
	message Control {
	required Settings.Command command = 1;
	optional string context = 2;
	};
	*/
	BOOST_FOREACH(const json_spirit::Object::value_type &node, json) {

	/*
	enum Command {
	LOAD	= 1;
	SAVE	= 2;
	RELOAD	= 3;
	};
*/
	if (node.second.isString() && node.first == "command") {
		std::string key = boost::to_upper_copy(node.second.getString());
		if (key == "LOAD")
			gpb->set_command(Plugin::Settings_Command_LOAD);
		else if (key == "SAVE")
			gpb->set_command(Plugin::Settings_Command_SAVE);
		else if (key == "RELOAD")
			gpb->set_command(Plugin::Settings_Command_RELOAD);
	}
//		DELEGATE_OBJ(gpb, node, node, parse_json_settings_common_command);
		SET_STR(gpb, node, context);
	}
}

void parse_json_settings_payload(Plugin::SettingsRequestMessage_Request* gpb, const json_spirit::Object &json) {
	BOOST_FOREACH(const json_spirit::Object::value_type &node, json) {
		SET_INT64(gpb, node, id);
		SET_INT(gpb, node, plugin_id);
		DELEGATE_OBJ(gpb, node, registration, parse_json_settings_payload_registration);
		DELEGATE_OBJ(gpb, node, query, parse_json_settings_payload_query);
		DELEGATE_OBJ(gpb, node, update, parse_json_settings_payload_update);
		DELEGATE_OBJ(gpb, node, control, parse_json_settings_payload_control);
	}
}

void parse_json_registry_payload(Plugin::RegistryRequestMessage_Request* gpb, const json_spirit::Object &json) {
	BOOST_FOREACH(const json_spirit::Object::value_type &node, json) {
		SET_INT64(gpb, node, id);
		//SET_INT(gpb, node, plugin_id);
		DELEGATE_OBJ(gpb, node, registration, parse_json_registry_payload_registration);
		//DELEGATE_OBJ(gpb, node, inventory, parse_json_settings_payload_query);
	}
}


std::string parse_json_settings_request(const json_spirit::Object &header, const std::list<json_spirit::Object> &payloads) {
	Plugin::SettingsRequestMessage request_message;
	parse_json_header(request_message.mutable_header(), header);
	BOOST_FOREACH(const json_spirit::Object &payload, payloads) {
		parse_json_settings_payload(request_message.add_payload(), payload);
	}
	return request_message.SerializeAsString();
}

std::string parse_json_registry_request(const json_spirit::Object &header, const std::list<json_spirit::Object> &payloads) {
	Plugin::RegistryRequestMessage request_message;
	parse_json_header(request_message.mutable_header(), header);
	BOOST_FOREACH(const json_spirit::Object &payload, payloads) {
		parse_json_registry_payload(request_message.add_payload(), payload);
	}
	return request_message.SerializeAsString();
}



NSCAPI::errorReturn NSCAPIJson2Protobuf(const char* request_buffer, unsigned int request_buffer_len, char ** response_buffer, unsigned int *response_buffer_len) {
	std::string request(request_buffer, request_buffer_len), response;
	try {
		json_spirit::Value root;
		json_spirit::read_or_throw(request, root);
		std::string object_type;
		json_spirit::Object header;
		std::list<json_spirit::Object> payloads;
		json_spirit::Object o = root.getObject();
		BOOST_FOREACH(const json_spirit::Object::value_type &p, o) {
			if (p.first == "type" && p.second.type() == json_spirit::Value::STRING_TYPE)
				object_type = p.second.getString();
			if (p.first == "payload" && p.second.isObject())
				payloads.push_back(p.second.getObject());
			if (p.first == "payload" && p.second.isArray()) {
				BOOST_FOREACH(const json_spirit::Value &payload, p.second.getArray()) {
					payloads.push_back(payload.getObject());
				}
			}
			if (p.first == "header" && p.second.isObject())
				header = p.second.getObject();
		}
		std::string response;
		if (object_type.empty() || payloads.size() == 0) {
			LOG_ERROR_STD("Missing type or payload.");
			return NSCAPI::hasFailed;
		} else if (object_type == "SettingsRequestMessage") {
			response = parse_json_settings_request(header, payloads);
		} else if (object_type == "RegistryRequestMessage") {
			response = parse_json_registry_request(header, payloads);
		} else {
			LOG_ERROR_STD("Missing type or payload.");
			return NSCAPI::hasFailed;
		}
		*response_buffer_len = static_cast<unsigned int>(response.size());
		if (response.empty())
			*response_buffer = NULL;
		else {
			*response_buffer = new char[*response_buffer_len + 10];
			memcpy(*response_buffer, response.c_str(), *response_buffer_len);
		}
	} catch (const json_spirit::ParseError &e) {
		LOG_ERROR_STD("Failed to parse JSON: " + e.reason_);
		return NSCAPI::hasFailed;
	}
	return NSCAPI::isSuccess;
}

#define SET_JSON_VALUE(gpb, node, tag) if (gpb.has_##tag()) { node.insert(json_spirit::Object::value_type(#tag, gpb.tag())); }
#define SET_JSON_VALUE_EX(gpb, node, name, key) if (gpb.has_##key()) { node.insert(json_spirit::Object::value_type(#name, gpb.key())); }
#define SET_JSON_VALUE_LIST(gpb, node, tag) if (gpb.tag ## _size() > 0) { json_spirit::Array arr; for (int i=0;i<gpb.tag ## _size();++i) { arr.push_back(json_spirit::Value(gpb.tag(i)));} node.insert(json_spirit::Object::value_type(#tag, arr)); }
#define SET_JSON_NILL(gpb, node, tag) if (gpb.has_##tag()) { node.insert(json_spirit::Object::value_type(#tag, json_spirit::Object())); }
#define SET_JSON_DELEGATE(gpb, node, tag, fun) if (gpb.has_##tag()) { node.insert(json_spirit::Object::value_type(#tag, fun(gpb.tag()))); }
#define SET_JSON_DELEGATE_LIST(gpb, node, tag, fun) if (gpb.tag ## _size() > 0) { json_spirit::Array arr; for (int i=0;i<gpb.tag ## _size();++i) { arr.push_back(json_spirit::Value(fun(gpb.tag(i))));} node.insert(json_spirit::Object::value_type(#tag, arr)); }

json_spirit::Object build_json_header(const ::Plugin::Common_Header& gpb) {
	json_spirit::Object node;
// 	if (node.second.type() == json_spirit::Value::STRING_TYPE) {
// 		if (node.first == "version") {
// 			if (node.second.getString() == "1")
// 				gpb->set_version(Plugin::Common_Version_VERSION_1);
// 			else
// 				LOG_ERROR_STD("Invalid version: " + node.second.getString());
// 		} else if (node.first == "max_supported_version") {
// 			if (node.second.getString() == "1")
// 				gpb->set_max_supported_version(Plugin::Common_Version_VERSION_1);
// 			else
// 				LOG_ERROR_STD("Invalid max_supported_version: " + node.second.getString());
// 		}
// 	}
	SET_JSON_VALUE(gpb, node, source_id);
	SET_JSON_VALUE(gpb, node, sender_id);
	SET_JSON_VALUE(gpb, node, recipient_id);
	SET_JSON_VALUE(gpb, node, destination_id);
	SET_JSON_VALUE(gpb, node, message_id);
	return node;
}
json_spirit::Object build_json_common_any_data(const Plugin::Common::AnyDataType &gpb) {
	json_spirit::Object node;
	if (gpb.type() == Plugin::Common_DataType_BOOL) {
		 node.insert(json_spirit::Object::value_type("type", "bool"));
	} else if (gpb.type() == Plugin::Common_DataType_INT) {
		node.insert(json_spirit::Object::value_type("type", "int"));
	} else if (gpb.type() == Plugin::Common_DataType_STRING) {
		node.insert(json_spirit::Object::value_type("type", "string"));
	} else if (gpb.type() == Plugin::Common_DataType_FLOAT) {
		node.insert(json_spirit::Object::value_type("type", "float"));
	} else if (gpb.type() == Plugin::Common_DataType_LIST) {
		node.insert(json_spirit::Object::value_type("type", "list"));
	}
	SET_JSON_VALUE_EX(gpb, node, value, string_data);
	SET_JSON_VALUE_EX(gpb, node, value, int_data);
	SET_JSON_VALUE_EX(gpb, node, value, float_data);
	SET_JSON_VALUE_EX(gpb, node, value, bool_data);
	SET_JSON_VALUE_EX(gpb, node, value, string_data);
	SET_JSON_VALUE_LIST(gpb, node, list_data);
	return node;
}

json_spirit::Object build_json_common_status(const Plugin::Common::Status &gpb) {
	json_spirit::Object node;
	SET_JSON_VALUE(gpb, node, status);
	SET_JSON_VALUE(gpb, node, message);
	SET_JSON_VALUE(gpb, node, data);
	return node;
}
json_spirit::Object build_json_settings_common_node(const Plugin::Settings::Node &gpb) {
	json_spirit::Object node;
	SET_JSON_VALUE(gpb, node, path);
	SET_JSON_VALUE(gpb, node, key);
	return node;
}
json_spirit::Object build_json_settings_common_info(const Plugin::Settings::Information &gpb) {
	json_spirit::Object node;
/*
message Information {
optional string title = 1;
optional string description = 2;
optional Common.AnyDataType default_value = 3;
optional string min_version = 4;
optional string max_version = 5;
optional bool advanced = 6;
optional bool sample = 7;
optional string sample_usage = 8;
repeated string plugin = 9;
};*/

	SET_JSON_VALUE(gpb, node, title);
	SET_JSON_VALUE(gpb, node, description);
	SET_JSON_DELEGATE(gpb, node, default_value, build_json_common_any_data);

	//SET_JSON_VALUE(gpb, node, default_value);
	SET_JSON_VALUE(gpb, node, min_version);
	SET_JSON_VALUE(gpb, node, max_version);
	SET_JSON_VALUE(gpb, node, advanced);
	SET_JSON_VALUE(gpb, node, sample);
	SET_JSON_VALUE(gpb, node, sample_usage);
	SET_JSON_VALUE_LIST(gpb, node, plugin);
	return node;
}

json_spirit::Object build_json_settings_response_payload_query(const Plugin::SettingsResponseMessage::Response::Query &gpb) {
	json_spirit::Object node;
	SET_JSON_DELEGATE(gpb, node, node, build_json_settings_common_node);
	SET_JSON_DELEGATE(gpb, node, value, build_json_common_any_data);
	return node;
}
/*
message Status {
optional string context = 1;
optional string type = 2;
optional bool has_changed = 3;
};
*/
json_spirit::Object build_json_settings_response_payload_status(const Plugin::SettingsResponseMessage::Response::Status &gpb) {
	json_spirit::Object node;
	SET_JSON_VALUE(gpb, node, context);
	SET_JSON_VALUE(gpb, node, type);
	SET_JSON_VALUE(gpb, node, has_changed);
	return node;
}
json_spirit::Object build_json_registry_response_payload_registration(const Plugin::RegistryResponseMessage::Response::Registration &gpb) {
	json_spirit::Object node;
	SET_JSON_VALUE(gpb, node, item_id);
	return node;
}

json_spirit::Object build_json_settings_response_payload_inventory(const Plugin::SettingsResponseMessage::Response::Inventory &gpb) {
	json_spirit::Object node;
	SET_JSON_DELEGATE(gpb, node, node, build_json_settings_common_node);
	SET_JSON_DELEGATE(gpb, node, info, build_json_settings_common_info);
	SET_JSON_DELEGATE(gpb, node, value, build_json_common_any_data);
	return node;
}


json_spirit::Object build_json_settings_response_payload(const Plugin::SettingsResponseMessage::Response &gpb) {
	json_spirit::Object node;
	SET_JSON_VALUE(gpb, node, id);
	SET_JSON_DELEGATE(gpb, node, result, build_json_common_status);
	SET_JSON_NILL(gpb, node, registration);
	SET_JSON_DELEGATE(gpb, node, query, build_json_settings_response_payload_query);
	SET_JSON_NILL(gpb, node, update);
	SET_JSON_DELEGATE_LIST(gpb, node, inventory, build_json_settings_response_payload_inventory);
	SET_JSON_NILL(gpb, node, control);
	SET_JSON_DELEGATE(gpb, node, status, build_json_settings_response_payload_status);
	return node;
}

/*

optional string title = 1;
optional string description = 2;

repeated Common.KeyValue metadata = 3;

optional string min_version = 5;
optional string max_version = 6;

optional bool advanced = 8;
repeated string plugin = 9;
*/

json_spirit::Object build_json_registry_response_payload_inventory_info(const Plugin::Registry::Information &gpb) {
	json_spirit::Object node;
	SET_JSON_VALUE(gpb, node, title);
	SET_JSON_VALUE(gpb, node, description);
	// TODO: metadata
	SET_JSON_VALUE(gpb, node, min_version);
	SET_JSON_VALUE(gpb, node, max_version);
	SET_JSON_VALUE(gpb, node, advanced);
	SET_JSON_VALUE_LIST(gpb, node, plugin);
	return node;
}

json_spirit::Object build_json_registry_response_payload_inventory(const Plugin::RegistryResponseMessage::Response::Inventory &gpb) {
	json_spirit::Object node;
	SET_JSON_VALUE_LIST(gpb, node, plugin);
	// type
	SET_JSON_VALUE(gpb, node, name);
	SET_JSON_DELEGATE(gpb, node, info, build_json_registry_response_payload_inventory_info);
//	SET_JSON_DELEGATE(gpb, node, result, build_json_common_status);
//	SET_JSON_NILL(gpb, node, registration);
//	SET_JSON_DELEGATE(gpb, node, query, build_json_settings_response_payload_query);
//	SET_JSON_NILL(gpb, node, update);
	//SET_JSON_DELEGATE_LIST(gpb, node, inventory);
//	SET_JSON_NILL(gpb, node, control);
	return node;
}

json_spirit::Object build_json_registry_response_payload(const Plugin::RegistryResponseMessage::Response &gpb) {
	json_spirit::Object node;
	SET_JSON_VALUE(gpb, node, id);
	SET_JSON_DELEGATE(gpb, node, result, build_json_common_status);
	SET_JSON_DELEGATE(gpb, node, registration, build_json_registry_response_payload_registration);
	SET_JSON_DELEGATE_LIST(gpb, node, inventory, build_json_registry_response_payload_inventory);
	return node;
}

json_spirit::Object build_json_query_response_payload(const Plugin::QueryResponseMessage::Response &gpb) {
	json_spirit::Object node;
	SET_JSON_VALUE(gpb, node, id);
	SET_JSON_VALUE(gpb, node, message);
	SET_JSON_VALUE(gpb, node, result);

	std::string perf = nscapi::protobuf::functions::build_performance_data(gpb);
	if (!perf.empty()) { node.insert(json_spirit::Object::value_type("perf", perf));}
	//SET_JSON_DELEGATE(gpb, node, result, build_json_common_status);
	//SET_JSON_DELEGATE(gpb, node, registration, build_json_registry_response_payload_registration);
	//SET_JSON_DELEGATE_LIST(gpb, node, inventory, build_json_registry_response_payload_inventory);
	return node;
}

NSCAPI::errorReturn NSCAPIProtobuf2Json(const char* object, const char* request_buffer, unsigned int request_buffer_len, char ** response_buffer, unsigned int *response_buffer_len) {
	std::string request(request_buffer, request_buffer_len), response, obj(object);
	try {
		json_spirit::Object root;
		if (obj == "SettingsResponseMessage") {
			Plugin::SettingsResponseMessage message;
			message.ParseFromString(request);
			root.insert(json_spirit::Object::value_type("type", obj));
			root.insert(json_spirit::Object::value_type("header", build_json_header(message.header())));
			if (message.payload_size() > 0) { 
				json_spirit::Array arr; 
				for (int i=0;i<message.payload_size();++i) { 
					arr.push_back(json_spirit::Value(build_json_settings_response_payload(message.payload(0))));
				} 
				root.insert(json_spirit::Object::value_type("payload", arr)); 
			}
		} else if (obj == "RegistryResponseMessage") {
			Plugin::RegistryResponseMessage message;
			message.ParseFromString(request);
			root.insert(json_spirit::Object::value_type("type", obj));
			root.insert(json_spirit::Object::value_type("header", build_json_header(message.header())));
			if (message.payload_size() != 1) {
				LOG_ERROR_STD("Invalid size: " + obj);
			} else {
				root.insert(json_spirit::Object::value_type("payload", build_json_registry_response_payload(message.payload(0))));
			}
		} else if (obj == "QueryResponseMessage") {
			Plugin::QueryResponseMessage message;
			message.ParseFromString(request);
			root.insert(json_spirit::Object::value_type("type", obj));
			root.insert(json_spirit::Object::value_type("header", build_json_header(message.header())));
			if (message.payload_size() != 1) {
				LOG_ERROR_STD("Invalid size: " + obj);
			} else {
				root.insert(json_spirit::Object::value_type("payload", build_json_query_response_payload(message.payload(0))));
			}
		} else {
			LOG_ERROR_STD("Invalid type: " + obj);
			return NSCAPI::hasFailed;
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
		LOG_ERROR_STD("Failed to parse JSON: " + e.reason_);
		return NSCAPI::hasFailed;
	}
	return NSCAPI::isSuccess;
}
#else
NSCAPI::errorReturn NSCAPIJson2protobuf(const char* request_buffer, unsigned int request_buffer_len, char ** response_buffer, unsigned int *response_buffer_len) {
	LOG_ERROR_STD("Not compiled with jason spirit so json not supported");
	return NSCAPI::hasFailed;
}
#endif

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
#include "StdAfx.h"
#include "NSClient++.h"
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

#pragma message("Hello")
#ifdef HAVE_JSON_SPIRIT
//#define JSON_SPIRIT_VALUE_ENABLED
#include <json_spirit.h>
#endif


using namespace nscp::helpers;

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
	*response_buffer_len = response.size();
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
	*response_buffer_len = response.size();
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

NSCAPI::errorReturn NSAPIEncrypt(unsigned int algorithm, const wchar_t* inBuffer, unsigned int inBufLen, wchar_t* outBuf, unsigned int *outBufLen) {
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

NSCAPI::errorReturn NSAPIDecrypt(unsigned int algorithm, const wchar_t* inBuffer, unsigned int inBufLen, wchar_t* outBuf, unsigned int *outBufLen) {
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
	int sz = str.size();
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

LPVOID NSAPILoader(const char* buffer) {
	if (strcmp(buffer, "NSAPIGetApplicationName") == 0)
		return reinterpret_cast<LPVOID>(&NSAPIGetApplicationName);
	if (strcmp(buffer, "NSAPIGetApplicationVersionStr") == 0)
		return reinterpret_cast<LPVOID>(&NSAPIGetApplicationVersionStr);
	if (strcmp(buffer, "NSAPIMessage") == 0)
		return reinterpret_cast<LPVOID>(&NSAPIMessage);
	if (strcmp(buffer, "NSAPISimpleMessage") == 0)
		return reinterpret_cast<LPVOID>(&NSAPISimpleMessage);
	if (strcmp(buffer, "NSAPIInject") == 0)
		return reinterpret_cast<LPVOID>(&NSAPIInject);
	if (strcmp(buffer, "NSAPIExecCommand") == 0)
		return reinterpret_cast<LPVOID>(&NSAPIExecCommand);
	if (strcmp(buffer, "NSAPICheckLogMessages") == 0)
		return reinterpret_cast<LPVOID>(&NSAPICheckLogMessages);
	if (strcmp(buffer, "NSAPIEncrypt") == 0)
		return reinterpret_cast<LPVOID>(&NSAPIEncrypt);
	if (strcmp(buffer, "NSAPIDecrypt") == 0)
		return reinterpret_cast<LPVOID>(&NSAPIDecrypt);
	if (strcmp(buffer, "NSAPINotify") == 0)
		return reinterpret_cast<LPVOID>(&NSAPINotify);
	if (strcmp(buffer, "NSAPIDestroyBuffer") == 0)
		return reinterpret_cast<LPVOID>(&NSAPIDestroyBuffer);
	if (strcmp(buffer, "NSAPIExpandPath") == 0)
		return reinterpret_cast<LPVOID>(&NSAPIExpandPath);
	if (strcmp(buffer, "NSAPIReload") == 0)
		return reinterpret_cast<LPVOID>(&NSAPIReload);
	if (strcmp(buffer, "NSAPIGetLoglevel") == 0)
		return reinterpret_cast<LPVOID>(&NSAPIGetLoglevel);
	if (strcmp(buffer, "NSAPISettingsQuery") == 0)
		return reinterpret_cast<LPVOID>(&NSAPISettingsQuery);
	if (strcmp(buffer, "NSAPIRegistryQuery") == 0)
		return reinterpret_cast<LPVOID>(&NSAPIRegistryQuery);
	if (strcmp(buffer, "NSCAPIJson2Protobuf") == 0)
		return reinterpret_cast<LPVOID>(&NSCAPIJson2Protobuf);
	if (strcmp(buffer, "NSCAPIProtobuf2Json") == 0)
		return reinterpret_cast<LPVOID>(&NSCAPIProtobuf2Json);

	LOG_ERROR_STD("Function not found: " + buffer);
	return NULL;
}

NSCAPI::errorReturn NSAPINotify(const char* channel, const char* request_buffer, unsigned int request_buffer_len, char ** response_buffer, unsigned int *response_buffer_len) {
	std::string request (request_buffer, request_buffer_len), response;
	NSCAPI::nagiosReturn ret = mainClient.send_notification(channel, request, response);
	*response_buffer_len = response.size();
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
#include <protobuf/plugin.pb.h>
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

#define SET_STR(obj, src, tag) if (src.value_.type() == json_spirit::str_type && src.name_ == # tag) { obj->set_ ## tag(src.value_.get_str()); continue; }
#define SET_BOOL(obj, src, tag) if (src.value_.type() == json_spirit::bool_type && src.name_ == # tag) { obj->set_ ## tag(src.value_.get_bool()); continue; }
#define SET_STR_LIST(obj, src, tag) if (src.value_.type() == json_spirit::str_type && src.name_ == # tag) { obj->add_ ## tag(src.value_.get_str()); continue; } \
	if (src.value_.type() == json_spirit::array_type && src.name_ == # tag) { BOOST_FOREACH(const json_spirit::Value &s, src.value_.get_array()) { if (s.type() == json_spirit::str_type) { obj->add_ ## tag(s.get_str());}} continue; }
#define SET_INT64(obj, src, tag) if (src.value_.type() == json_spirit::int_type && src.name_ == # tag) { obj->set_ ## tag(src.value_.get_int64()); continue; }
#define SET_INT(obj, src, tag) if (src.value_.type() == json_spirit::int_type && src.name_ == # tag) { obj->set_ ## tag(src.value_.get_int()); continue; }
#define DELEGATE_OBJ(obj, src, tag, fun) if (src.value_.type() == json_spirit::obj_type && src.name_ == # tag) { fun(obj->mutable_ ## tag(), src.value_.get_obj()); continue; }


void parse_json_header(Plugin::Common_Header* gpb, const json_spirit::Object &json) {
	BOOST_FOREACH(const json_spirit::Pair &node, json) {
		if (node.value_.type() == json_spirit::str_type) {
			if (node.name_ == "version") {
				if (node.value_.get_str() == "1")
					gpb->set_version(Plugin::Common_Version_VERSION_1);
				else
					LOG_ERROR_STD("Invalid version: " + node.value_.get_str());
			} else if (node.name_ == "max_supported_version") {
				if (node.value_.get_str() == "1")
					gpb->set_max_supported_version(Plugin::Common_Version_VERSION_1);
				else
					LOG_ERROR_STD("Invalid max_supported_version: " + node.value_.get_str());
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
	BOOST_FOREACH(const json_spirit::Pair &node, json) {
		SET_STR(gpb, node, path);
		SET_STR(gpb, node, key);
	}
}

template<class T>
void parse_json_settings_common_type(T* gpb, const json_spirit::Pair &node, const std::string &alias) {
	if (node.value_.type() == json_spirit::str_type && node.name_ == alias) {
		if (node.value_ == "INT")
			gpb->set_type(Plugin::Common_DataType_INT);
		else if (node.value_ == "STRING")
			gpb->set_type(Plugin::Common_DataType_STRING);
		else if (node.value_ == "FLOAT")
			gpb->set_type(Plugin::Common_DataType_FLOAT);
		else if (node.value_ == "BOOL")
			gpb->set_type(Plugin::Common_DataType_BOOL);
		else if (node.value_ == "LIST")
			gpb->set_type(Plugin::Common_DataType_LIST);
	}
}

template<class T>
void parse_json_registry_common_type(T* gpb, const json_spirit::Pair &node, const std::string &alias) {
	if (node.value_.type() == json_spirit::str_type && node.name_ == alias) {
		if (node.value_ == "QUERY")
			gpb->set_type(Plugin::Registry::QUERY);
		else if (node.value_ == "COMMAND")
			gpb->set_type(Plugin::Registry::COMMAND);
		else if (node.value_ == "HANDLER")
			gpb->set_type(Plugin::Registry::HANDLER);
		else if (node.value_ == "PLUGIN")
			gpb->set_type(Plugin::Registry::PLUGIN);
		else if (node.value_ == "QUERY_ALIAS")
			gpb->set_type(Plugin::Registry::QUERY_ALIAS);
		else if (node.value_ == "ROUTER")
			gpb->set_type(Plugin::Registry::ROUTER);
		else if (node.value_ == "ALL")
			gpb->set_type(Plugin::Registry::ALL);
	}
}

void parse_json_common_any_data(Plugin::Common::AnyDataType* gpb, const json_spirit::Object &json) {
	BOOST_FOREACH(const json_spirit::Pair &node, json) {
		parse_json_settings_common_type(gpb, node, "type");
		SET_STR(gpb, node, string_data);
		SET_INT64(gpb, node, int_data);
		//SET_FLOAT(gpb, node, float_data);
		SET_BOOL(gpb, node, bool_data);
		SET_STR(gpb, node, string_data);
		SET_STR_LIST(gpb, node, list_data);
	}
}

void parse_json_settings_common_info(Plugin::Settings::Information* gpb, const json_spirit::Object &json) {
	BOOST_FOREACH(const json_spirit::Pair &node, json) {
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
	BOOST_FOREACH(const json_spirit::Pair &node, json) {
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
	BOOST_FOREACH(const json_spirit::Pair &node, json) {
		DELEGATE_OBJ(gpb, node, node, parse_json_settings_common_node);
		DELEGATE_OBJ(gpb, node, info, parse_json_settings_common_info);
	}
}
void parse_json_registry_payload_registration(Plugin::RegistryRequestMessage::Request::Registration* gpb, const json_spirit::Object &json) {
	BOOST_FOREACH(const json_spirit::Pair &node, json) {
		SET_INT(gpb, node, plugin_id);
		parse_json_registry_common_type(gpb, node, "type");
		SET_STR(gpb, node, name);
		DELEGATE_OBJ(gpb, node, info, parse_json_registry_common_info);
		SET_STR_LIST(gpb, node, alias);
	}
}
void parse_json_settings_payload_query(Plugin::SettingsRequestMessage::Request::Query* gpb, const json_spirit::Object &json) {
	BOOST_FOREACH(const json_spirit::Pair &node, json) {
		DELEGATE_OBJ(gpb, node, node, parse_json_settings_common_node);
		// TODO:; QUery
		SET_BOOL(gpb, node, recursive);
		parse_json_settings_common_type(gpb, node, "type");
		DELEGATE_OBJ(gpb, node, default_value, parse_json_common_any_data);
	}
}
void parse_json_settings_payload_update(Plugin::SettingsRequestMessage::Request::Update* gpb, const json_spirit::Object &json) {
	BOOST_FOREACH(const json_spirit::Pair &node, json) {
		DELEGATE_OBJ(gpb, node, node, parse_json_settings_common_node);
		DELEGATE_OBJ(gpb, node, value, parse_json_common_any_data);
	}
}

void parse_json_settings_payload(Plugin::SettingsRequestMessage_Request* gpb, const json_spirit::Object &json) {
	BOOST_FOREACH(const json_spirit::Pair &node, json) {
		SET_INT64(gpb, node, id);
		SET_INT(gpb, node, plugin_id);
		DELEGATE_OBJ(gpb, node, registration, parse_json_settings_payload_registration);
		DELEGATE_OBJ(gpb, node, query, parse_json_settings_payload_query);
		DELEGATE_OBJ(gpb, node, update, parse_json_settings_payload_update);
	}
}

void parse_json_registry_payload(Plugin::RegistryRequestMessage_Request* gpb, const json_spirit::Object &json) {
	BOOST_FOREACH(const json_spirit::Pair &node, json) {
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
		json_spirit::Object o = root.get_obj();
		BOOST_FOREACH(const json_spirit::Pair &p, o) {
			if (p.name_ == "type" && p.value_.type() == json_spirit::str_type)
				object_type = p.value_.get_str();
			if (p.name_ == "payload" && p.value_.type() == json_spirit::obj_type)
				payloads.push_back(p.value_.get_obj());
			if (p.name_ == "header" && p.value_.type() == json_spirit::obj_type)
				header = p.value_.get_obj();
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
		*response_buffer_len = response.size();
		if (response.empty())
			*response_buffer = NULL;
		else {
			*response_buffer = new char[*response_buffer_len + 10];
			memcpy(*response_buffer, response.c_str(), *response_buffer_len);
		}
	} catch (const json_spirit::Error_position &e) {
		LOG_ERROR_STD("Failed to parse JSON: " + e.reason_);
		return NSCAPI::hasFailed;
	}
	return NSCAPI::isSuccess;
}

#define SET_JSON_VALUE(gpb, node, tag) if (gpb.has_##tag()) { node.push_back(json_spirit::Pair(#tag, gpb.tag())); }
#define SET_JSON_VALUE_LIST(gpb, node, tag) if (gpb.tag ## _size() > 0) { json_spirit::Array arr; for (int i=0;i<gpb.tag ## _size();++i) { arr.push_back(json_spirit::Value(gpb.tag(i)));} node.push_back(json_spirit::Pair(#tag, arr)); }
#define SET_JSON_NILL(gpb, node, tag) if (gpb.has_##tag()) { node.push_back(json_spirit::Pair(#tag, json_spirit::Object())); }
#define SET_JSON_DELEGATE(gpb, node, tag, fun) if (gpb.has_##tag()) { node.push_back(json_spirit::Pair(#tag, fun(gpb.tag()))); }

json_spirit::Object build_json_header(const ::Plugin::Common_Header& gpb) {
	json_spirit::Object node;
// 	if (node.value_.type() == json_spirit::str_type) {
// 		if (node.name_ == "version") {
// 			if (node.value_.get_str() == "1")
// 				gpb->set_version(Plugin::Common_Version_VERSION_1);
// 			else
// 				LOG_ERROR_STD("Invalid version: " + node.value_.get_str());
// 		} else if (node.name_ == "max_supported_version") {
// 			if (node.value_.get_str() == "1")
// 				gpb->set_max_supported_version(Plugin::Common_Version_VERSION_1);
// 			else
// 				LOG_ERROR_STD("Invalid max_supported_version: " + node.value_.get_str());
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
	//parse_json_settings_common_type(gpb, node, "type");
	SET_JSON_VALUE(gpb, node, string_data);
	SET_JSON_VALUE(gpb, node, int_data);
	SET_JSON_VALUE(gpb, node, float_data);
	SET_JSON_VALUE(gpb, node, bool_data);
	SET_JSON_VALUE(gpb, node, string_data);
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

json_spirit::Object build_json_settings_response_payload_query(const Plugin::SettingsResponseMessage::Response::Query &gpb) {
	json_spirit::Object node;
	SET_JSON_DELEGATE(gpb, node, node, build_json_settings_common_node);
	SET_JSON_DELEGATE(gpb, node, value, build_json_common_any_data);
	return node;
}
json_spirit::Object build_json_registry_response_payload_registration(const Plugin::RegistryResponseMessage::Response::Registration &gpb) {
	json_spirit::Object node;
	SET_JSON_VALUE(gpb, node, item_id);
	return node;
}

json_spirit::Object build_json_settings_response_payload(const Plugin::SettingsResponseMessage::Response &gpb) {
	json_spirit::Object node;
	SET_JSON_VALUE(gpb, node, id);
	SET_JSON_DELEGATE(gpb, node, result, build_json_common_status);
	SET_JSON_NILL(gpb, node, registration);
	SET_JSON_DELEGATE(gpb, node, query, build_json_settings_response_payload_query);
	SET_JSON_NILL(gpb, node, update);
	//SET_JSON_DELEGATE_LIST(gpb, node, inventory);
	SET_JSON_NILL(gpb, node, control);
	return node;
}
json_spirit::Object build_json_query_response_payload(const Plugin::RegistryResponseMessage::Response &gpb) {
	json_spirit::Object node;
	SET_JSON_VALUE(gpb, node, id);
	SET_JSON_DELEGATE(gpb, node, result, build_json_common_status);
	SET_JSON_DELEGATE(gpb, node, registration, build_json_registry_response_payload_registration);
	//SET_JSON_DELEGATE_LIST(gpb, node, inventory, build_json_settings_response_payload_inventory);
	return node;
}

NSCAPI::errorReturn NSCAPIProtobuf2Json(const char* object, const char* request_buffer, unsigned int request_buffer_len, char ** response_buffer, unsigned int *response_buffer_len) {
	std::string request(request_buffer, request_buffer_len), response, obj(object);
	try {
		json_spirit::Object root;
		if (obj == "SettingsResponseMessage") {
			Plugin::SettingsResponseMessage message;
			message.ParseFromString(request);
			root.push_back(json_spirit::Pair("type", obj));
			root.push_back(json_spirit::Pair("header", build_json_header(message.header())));
			if (message.payload_size() != 1) {
				LOG_ERROR_STD("Invalid size: " + obj);
			} else {
				root.push_back(json_spirit::Pair("payload", build_json_settings_response_payload(message.payload(0))));
			}
		} else if (obj == "RegistryResponseMessage") {
			Plugin::RegistryResponseMessage message;
			message.ParseFromString(request);
			root.push_back(json_spirit::Pair("type", obj));
			root.push_back(json_spirit::Pair("header", build_json_header(message.header())));
			if (message.payload_size() != 1) {
				LOG_ERROR_STD("Invalid size: " + obj);
			} else {
				root.push_back(json_spirit::Pair("payload", build_json_query_response_payload(message.payload(0))));
			}
		} else {
			LOG_ERROR_STD("Invalid type: " + obj);
			return NSCAPI::hasFailed;
		}
		std::string response = json_spirit::write(root);
		*response_buffer_len = response.size();
		if (response.empty())
			*response_buffer = NULL;
		else {
			*response_buffer = new char[*response_buffer_len + 10];
			memcpy(*response_buffer, response.c_str(), *response_buffer_len);
		}
	} catch (const json_spirit::Error_position &e) {
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

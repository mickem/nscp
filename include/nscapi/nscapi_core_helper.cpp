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

#include <nscapi/macros.hpp>
#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_protobuf_nagios.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/nscapi_plugin_wrapper.hpp>
#include <nscapi/nscapi_core_helper.hpp>

#include <utf8.hpp>

#include <boost/foreach.hpp>
#include <boost/tokenizer.hpp>

#define CORE_LOG_ERROR(msg) get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, msg);
#define CORE_LOG_ERROR_EX(msg) get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Exception in: " + msg);
#define CORE_LOG_ERROR_EXR(msg, ex) get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, std::string("Exception in: ") + msg + utf8::utf8_from_native(ex.what()));

const nscapi::core_wrapper* nscapi::core_helper::get_core() {
	return core_;
}

nscapi::core_helper::storage_map nscapi::core_helper::get_storage_strings(std::string context) {
	storage_map ret;
	Plugin::StorageRequestMessage rrm;
	Plugin::StorageRequestMessage::Request *payload = rrm.add_payload();

	payload->mutable_get()->set_context(context);
	//payload->mutable_put()->mutable_entry()->set_key(context);
	std::string buffer;
	get_core()->storage_query(rrm.SerializeAsString(), buffer);

	Plugin::StorageResponseMessage resp_msg;
	resp_msg.ParseFromString(buffer);
	BOOST_FOREACH(const ::Plugin::StorageResponseMessage::Response &payload, resp_msg.payload()) {
		if (payload.result().code() != Plugin::Common_Result_StatusCodeType_STATUS_OK) {
			CORE_LOG_ERROR("Failed to store data " + context + ": " + payload.result().message());
		} else {
			BOOST_FOREACH(const ::Plugin::Storage::Entry &e, payload.get().entry()) {
				if (e.value().has_string_data()) {
					ret[e.key()] = e.value().string_data();
				} else {
					CORE_LOG_ERROR("Add support for non string type.");
				}
			}
		}
	}
	return ret;
}

bool nscapi::core_helper::put_storage(std::string context, std::string key, std::string value, bool private_data, bool binary_data) {
	Plugin::StorageRequestMessage rrm;
	Plugin::StorageRequestMessage::Request *payload = rrm.add_payload();

	payload->set_plugin_id(plugin_id_);
	payload->mutable_put()->mutable_entry()->set_context(context);
	payload->mutable_put()->mutable_entry()->set_key(key);
	payload->mutable_put()->mutable_entry()->mutable_value()->set_string_data(value);
	payload->mutable_put()->mutable_entry()->set_private_data(private_data);
	payload->mutable_put()->mutable_entry()->set_binary_data(binary_data);
	std::string buffer;
	get_core()->storage_query(rrm.SerializeAsString(), buffer);

	Plugin::StorageResponseMessage resp_msg;
	resp_msg.ParseFromString(buffer);
	bool ret = true;
	BOOST_FOREACH(const ::Plugin::StorageResponseMessage::Response &payload, resp_msg.payload()) {
		if (payload.result().code() != Plugin::Common_Result_StatusCodeType_STATUS_OK) {
			CORE_LOG_ERROR("Failed to store data " + context + ": " + payload.result().message());
			ret = false;
		}
	}
	return ret;
}

bool nscapi::core_helper::load_module(std::string name, std::string alias) {
	Plugin::RegistryRequestMessage rrm;
	Plugin::RegistryRequestMessage::Request *payload = rrm.add_payload();

	payload->mutable_control()->set_type(Plugin::Registry_ItemType_MODULE);
	payload->mutable_control()->set_command(Plugin::Registry_Command_LOAD);
	payload->mutable_control()->set_name(name);
	if (!alias.empty()) {
		payload->mutable_control()->set_alias(alias);
	}
	std::string buffer;
	get_core()->registry_query(rrm.SerializeAsString(), buffer);

	Plugin::RegistryResponseMessage resp_msg;
	resp_msg.ParseFromString(buffer);
	BOOST_FOREACH(const ::Plugin::RegistryResponseMessage_Response &payload, resp_msg.payload()) {
		if (payload.result().code() == Plugin::Common_Result_StatusCodeType_STATUS_OK) {
			return true;
		} else {
			CORE_LOG_ERROR("Failed to load " + name + ": " + payload.result().message());
		}
	}
	return false;
}

bool nscapi::core_helper::unload_module(std::string name) {
	Plugin::RegistryRequestMessage rrm;
	Plugin::RegistryRequestMessage::Request *payload = rrm.add_payload();

	payload->mutable_control()->set_type(Plugin::Registry_ItemType_MODULE);
	payload->mutable_control()->set_command(Plugin::Registry_Command_UNLOAD);
	payload->mutable_control()->set_name(name);
	std::string buffer;
	get_core()->registry_query(rrm.SerializeAsString(), buffer);

	Plugin::RegistryResponseMessage resp_msg;
	resp_msg.ParseFromString(buffer);
	BOOST_FOREACH(const ::Plugin::RegistryResponseMessage_Response &payload, resp_msg.payload()) {
		if (payload.result().code() == Plugin::Common_Result_StatusCodeType_STATUS_OK) {
			return true;
		} else {
			CORE_LOG_ERROR("Failed to load " + name + ": " + payload.result().message());
		}
	}
	return false;
}

bool nscapi::core_helper::submit_simple_message(const std::string channel, const std::string source_id, const std::string target_id, const std::string command, const NSCAPI::nagiosReturn code, const std::string & message, const std::string & perf, std::string & response) {
	std::string request, buffer;

	Plugin::SubmitRequestMessage request_message;
	request_message.mutable_header()->set_sender_id(source_id);
	request_message.mutable_header()->set_source_id(source_id);
	request_message.mutable_header()->set_recipient_id(target_id);
	request_message.mutable_header()->set_destination_id(target_id);
	request_message.set_channel(channel);

	Plugin::QueryResponseMessage::Response *payload = request_message.add_payload();
	payload->set_command(command);
	payload->set_result(nscapi::protobuf::functions::nagios_status_to_gpb(code));
	Plugin::QueryResponseMessage::Response::Line *line = payload->add_lines();
	line->set_message(message);
	if (!perf.empty())
		nscapi::protobuf::functions::parse_performance_data(line, perf);

	request_message.SerializeToString(&request);

	if (!get_core()->submit_message(channel, request, buffer)) {
		response = "Failed to submit message: " + channel;
		return false;
	}
	nscapi::protobuf::functions::parse_simple_submit_response(buffer, response);
	return true;
}

bool nscapi::core_helper::emit_event(const std::string module, const std::string event, std::list<std::map<std::string, std::string> > data, std::string &error) {
	std::string request, buffer;

	Plugin::EventMessage request_message;

	typedef std::list<std::map<std::string, std::string> > list_type;
	typedef std::map<std::string, std::string> hash_type;

	BOOST_FOREACH(const list_type::value_type &v, data) {
		Plugin::EventMessage::Request *payload = request_message.add_payload();

		payload->set_event(module + ":" + event);
		BOOST_FOREACH(const hash_type::value_type &e, v) {
			Plugin::Common::KeyValue *kv = payload->mutable_data()->Add();
			kv->set_key(e.first);
			kv->set_value(e.second);
		}
	}
	request_message.SerializeToString(&request);

	if (!get_core()->emit_event(request.c_str(), request.size())) {
		error = "Failed to emit event: " + event;
		return false;
	}
	return true;
}

bool nscapi::core_helper::emit_event(const std::string module, const std::string event, std::map<std::string, std::string> data, std::string &error) {
	std::string request, buffer;

	Plugin::EventMessage request_message;

	typedef std::map<std::string, std::string> hash_type;

	Plugin::EventMessage::Request *payload = request_message.add_payload();

	payload->set_event(module + ":" + event);
	BOOST_FOREACH(const hash_type::value_type &e, data) {
		Plugin::Common::KeyValue *kv = payload->mutable_data()->Add();
		kv->set_key(e.first);
		kv->set_value(e.second);
	}
	request_message.SerializeToString(&request);

	if (!get_core()->emit_event(request.c_str(), request.size())) {
		error = "Failed to emit event: " + event;
		return false;
	}
	return true;
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
NSCAPI::nagiosReturn nscapi::core_helper::simple_query(const std::string command, const std::list<std::string> & argument, std::string & msg, std::string & perf, std::size_t max_length) {
	std::string response;
	simple_query(command, argument, response);
	if (!response.empty()) {
		try {
			return nscapi::protobuf::functions::parse_simple_query_response(response, msg, perf, max_length);
		} catch (std::exception &e) {
			CORE_LOG_ERROR_EXR("Failed to extract return message: ", e);
			return NSCAPI::query_return_codes::returnUNKNOWN;
		}
	}
	return NSCAPI::query_return_codes::returnUNKNOWN;
}

bool nscapi::core_helper::simple_query(const std::string command, const std::list<std::string> & arguments, std::string & result) {
	std::string request;
	try {
		nscapi::protobuf::functions::create_simple_query_request(command, arguments, request);
	} catch (std::exception &e) {
		CORE_LOG_ERROR_EXR("Failed to extract return message: ", e);
		return NSCAPI::query_return_codes::returnUNKNOWN;
	}
	return get_core()->query(request, result);
}
bool nscapi::core_helper::simple_query(const std::string command, const std::vector<std::string> & arguments, std::string & result) {
	std::string request;
	try {
		nscapi::protobuf::functions::create_simple_query_request(command, arguments, request);
	} catch (std::exception &e) {
		CORE_LOG_ERROR_EXR("Failed to extract return message", e);
		return NSCAPI::query_return_codes::returnUNKNOWN;
	}
	return get_core()->query(request, result);
}

NSCAPI::nagiosReturn nscapi::core_helper::simple_query_from_nrpe(const std::string command, const std::string & buffer, std::string & message, std::string & perf, std::size_t max_length) {
	boost::tokenizer<boost::char_separator<char>, std::string::const_iterator, std::string > tok(buffer, boost::char_separator<char>("!"));
	std::list<std::string> arglist;
	BOOST_FOREACH(std::string s, tok)
		arglist.push_back(s);

	std::string response;
	simple_query(command, arglist, response);
	if (!response.empty()) {
		try {
			return nscapi::protobuf::functions::parse_simple_query_response(response, message, perf, max_length);
		} catch (std::exception &e) {
			CORE_LOG_ERROR_EXR("Failed to extract return message: ", e);
			return NSCAPI::query_return_codes::returnUNKNOWN;
		}
	}
	return NSCAPI::query_return_codes::returnUNKNOWN;
}

NSCAPI::nagiosReturn nscapi::core_helper::exec_simple_command(const std::string target, const std::string command, const std::list<std::string> &argument, std::list<std::string> & result) {
	std::string request, response;
	nscapi::protobuf::functions::create_simple_exec_request(target, command, argument, request);
	get_core()->exec_command(target, request, response);
	return nscapi::protobuf::functions::parse_simple_exec_response(response, result);
}

void nscapi::core_helper::register_command(std::string command, std::string description, std::list<std::string> aliases) {
	Plugin::RegistryRequestMessage request;

	Plugin::RegistryRequestMessage::Request *payload = request.add_payload();
	Plugin::RegistryRequestMessage::Request::Registration *regitem = payload->mutable_registration();
	regitem->set_plugin_id(plugin_id_);
	regitem->set_type(Plugin::Registry_ItemType_QUERY);
	regitem->set_name(command);
	regitem->mutable_info()->set_title(command);
	regitem->mutable_info()->set_description(description);
	BOOST_FOREACH(const std::string &alias, aliases) {
		regitem->add_alias(alias);
	}
	std::string response_string;
	get_core()->registry_query(request.SerializeAsString(), response_string);
	Plugin::RegistryResponseMessage response;
	response.ParseFromString(response_string);
	for (int i = 0; i < response.payload_size(); i++) {
		if (response.payload(i).result().code() != Plugin::Common_Result_StatusCodeType_STATUS_OK)
			get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Failed to register " + command + ": " + response.payload(i).result().message());
	}
}

void nscapi::core_helper::unregister_command(std::string command) {
	Plugin::RegistryRequestMessage request;

	Plugin::RegistryRequestMessage::Request *payload = request.add_payload();
	Plugin::RegistryRequestMessage::Request::Registration *regitem = payload->mutable_registration();
	regitem->set_plugin_id(plugin_id_);
	regitem->set_type(Plugin::Registry_ItemType_QUERY);
	regitem->set_name(command);
	regitem->set_unregister(true);
	regitem->mutable_info()->set_title(command);
	std::string response_string;
	get_core()->registry_query(request.SerializeAsString(), response_string);
	Plugin::RegistryResponseMessage response;
	response.ParseFromString(response_string);
	for (int i = 0; i < response.payload_size(); i++) {
		if (response.payload(i).result().code() != Plugin::Common_Result_StatusCodeType_STATUS_OK)
			get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Failed to unregister " + command + ": " + response.payload(i).result().message());
	}
}

void nscapi::core_helper::register_alias(std::string command, std::string description, std::list<std::string> aliases) {
	Plugin::RegistryRequestMessage request;

	Plugin::RegistryRequestMessage::Request *payload = request.add_payload();
	Plugin::RegistryRequestMessage::Request::Registration *regitem = payload->mutable_registration();
	regitem->set_plugin_id(plugin_id_);
	regitem->set_type(Plugin::Registry_ItemType_QUERY_ALIAS);
	regitem->set_name(command);
	regitem->mutable_info()->set_title(command);
	regitem->mutable_info()->set_description(description);
	BOOST_FOREACH(const std::string &alias, aliases) {
		regitem->add_alias(alias);
	}
	std::string response_string;
	get_core()->registry_query(request.SerializeAsString(), response_string);
	Plugin::RegistryResponseMessage response;
	response.ParseFromString(response_string);
	for (int i = 0; i < response.payload_size(); i++) {
		if (response.payload(i).result().code() != Plugin::Common_Result_StatusCodeType_STATUS_OK)
			get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Failed to register " + command + ": " + response.payload(i).result().message());
	}
}

void nscapi::core_helper::register_channel(const std::string channel) {
	Plugin::RegistryRequestMessage request;

	Plugin::RegistryRequestMessage::Request *payload = request.add_payload();
	Plugin::RegistryRequestMessage::Request::Registration *regitem = payload->mutable_registration();
	regitem->set_plugin_id(plugin_id_);
	regitem->set_type(Plugin::Registry_ItemType_HANDLER);
	regitem->set_name(channel);
	regitem->mutable_info()->set_title(channel);
	regitem->mutable_info()->set_description("Handler for: " + channel);
	std::string response_string;
	get_core()->registry_query(request.SerializeAsString(), response_string);
	Plugin::RegistryResponseMessage response;
	response.ParseFromString(response_string);
	for (int i = 0; i < response.payload_size(); i++) {
		if (response.payload(i).result().code() != Plugin::Common_Result_StatusCodeType_STATUS_OK)
			get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Failed to register " + channel + ": " + response.payload(i).result().message());
	}
}

void nscapi::core_helper::register_event(const std::string event) {
	Plugin::RegistryRequestMessage request;

	Plugin::RegistryRequestMessage::Request *payload = request.add_payload();
	Plugin::RegistryRequestMessage::Request::Registration *regitem = payload->mutable_registration();
	regitem->set_plugin_id(plugin_id_);
	regitem->set_type(Plugin::Registry_ItemType_EVENT);
	regitem->set_name(event);
	regitem->mutable_info()->set_title(event);
	regitem->mutable_info()->set_description("Handler for: " + event);
	std::string response_string;
	get_core()->registry_query(request.SerializeAsString(), response_string);
	Plugin::RegistryResponseMessage response;
	response.ParseFromString(response_string);
	for (int i = 0; i < response.payload_size(); i++) {
		if (response.payload(i).result().code() != Plugin::Common_Result_StatusCodeType_STATUS_OK)
			get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Failed to register " + event + ": " + response.payload(i).result().message());
	}
}
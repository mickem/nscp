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

#include <nscapi/macros.hpp>


#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/nscapi_plugin_wrapper.hpp>
#include <nscapi/nscapi_core_helper.hpp>

#include <protobuf/plugin.pb.h>
#include <nscapi/nscapi_protobuf_functions.hpp>

#define CORE_LOG_ERROR(msg) get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, msg);
#define CORE_LOG_ERROR_EX(msg) get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Exception in: " + msg);
#define CORE_LOG_ERROR_EXR(msg, ex) get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, std::string("Exception in: ") + msg + utf8::utf8_from_native(ex.what()));

extern nscapi::helper_singleton* nscapi::plugin_singleton;

nscapi::core_wrapper* get_core() {
	return nscapi::plugin_singleton->get_core();
}

bool nscapi::core_helper::submit_simple_message(const std::string channel, const std::string command, const NSCAPI::nagiosReturn code, const std::string & message, const std::string & perf, std::string & response) {
	std::string request, buffer;
	nscapi::protobuf::functions::create_simple_submit_request(channel, command, code, message, perf, request);
	NSCAPI::nagiosReturn ret = get_core()->submit_message(channel, request, buffer);
	if (ret == NSCAPI::returnIgnored) {
		response = "No handler for this message";
		return false;
	}
	if (buffer.size() == 0) {
		response = "Missing response from submission";
		return false;
	}
	nscapi::protobuf::functions::parse_simple_submit_response(buffer, response);
	return ret == NSCAPI::isSuccess;
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
NSCAPI::nagiosReturn nscapi::core_helper::simple_query(const std::string command, const std::list<std::string> & argument, std::string & msg, std::string & perf) 
{
	std::string response;
	NSCAPI::nagiosReturn ret = simple_query(command, argument, response);
	if (!response.empty()) {
		try {
			return nscapi::protobuf::functions::parse_simple_query_response(response, msg, perf);
		} catch (std::exception &e) {
			CORE_LOG_ERROR_EXR("Failed to extract return message: ", e);
			return NSCAPI::returnUNKNOWN;
		}
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
// NSCAPI::nagiosReturn nscapi::core_helper::simple_query(const std::wstring command, const std::list<std::wstring> & arguments, std::string & result) 
// {
// 	std::string request;
// 	try {
// 		nscapi::protobuf::functions::create_simple_query_request(command, arguments, request);
// 	} catch (const std::exception &e) {
// 		CORE_LOG_ERROR_EXR("Failed to extract return message: ", e);
// 		return NSCAPI::returnUNKNOWN;
// 	}
// 	return get_core()->query(command.c_str(), request, result);
// }

NSCAPI::nagiosReturn nscapi::core_helper::simple_query(const std::string command, const std::list<std::string> & arguments, std::string & result) 
{
	std::string request;
	try {
		nscapi::protobuf::functions::create_simple_query_request(command, arguments, request);
	} catch (std::exception &e) {
		CORE_LOG_ERROR_EXR("Failed to extract return message: ", e);
		return NSCAPI::returnUNKNOWN;
	}
	return get_core()->query(command.c_str(), request, result);
}
NSCAPI::nagiosReturn nscapi::core_helper::simple_query(const std::string command, const std::vector<std::string> & arguments, std::string & result) 
{
	std::string request;
	try {
		nscapi::protobuf::functions::create_simple_query_request(command, arguments, request);
	} catch (std::exception &e) {
		CORE_LOG_ERROR_EXR("Failed to extract return message", e);
		return NSCAPI::returnUNKNOWN;
	}
	return get_core()->query(command.c_str(), request, result);
}

NSCAPI::nagiosReturn nscapi::core_helper::simple_query_from_nrpe(const std::string command, const std::string & buffer, std::string & message, std::string & perf) {
	boost::tokenizer<boost::char_separator<char>, std::string::const_iterator, std::string > tok(buffer, boost::char_separator<char>("!"));
	std::list<std::string> arglist;
	BOOST_FOREACH(std::string s, tok)
		arglist.push_back(s);
	return simple_query(command, arglist, message, perf);
}

NSCAPI::nagiosReturn nscapi::core_helper::exec_simple_command(const std::string target, const std::string command, const std::list<std::string> &argument, std::list<std::string> & result) {
	std::string request, response;
	nscapi::protobuf::functions::create_simple_exec_request(command, argument, request);
	NSCAPI::nagiosReturn ret = get_core()->exec_command(target, command, request, response);
	nscapi::protobuf::functions::parse_simple_exec_response(response, result);
	return ret;
}



void nscapi::core_helper::core_proxy::register_command(std::string command, std::string description, std::list<std::string> aliases) {

	Plugin::RegistryRequestMessage request;
	nscapi::protobuf::functions::create_simple_header(request.mutable_header());

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
	nscapi::plugin_singleton->get_core()->registry_query(request.SerializeAsString(), response_string);
	Plugin::RegistryResponseMessage response;
	response.ParseFromString(response_string);
	for (int i=0;i<response.payload_size();i++) {
		if (response.payload(i).result().status() != Plugin::Common_Status_StatusType_STATUS_OK)
			nscapi::plugin_singleton->get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Failed to register " + command + ": " + response.payload(i).result().message());
	}
}

void nscapi::core_helper::core_proxy::register_channel(const std::string channel)
{
	Plugin::RegistryRequestMessage request;
	nscapi::protobuf::functions::create_simple_header(request.mutable_header());

	Plugin::RegistryRequestMessage::Request *payload = request.add_payload();
	Plugin::RegistryRequestMessage::Request::Registration *regitem = payload->mutable_registration();
	regitem->set_plugin_id(plugin_id_);
	regitem->set_type(Plugin::Registry_ItemType_HANDLER);
	regitem->set_name(channel);
	regitem->mutable_info()->set_title(channel);
	regitem->mutable_info()->set_description("Handler for: " + channel);
	std::string response_string;
	nscapi::plugin_singleton->get_core()->registry_query(request.SerializeAsString(), response_string);
	Plugin::RegistryResponseMessage response;
	response.ParseFromString(response_string);
	for (int i=0;i<response.payload_size();i++) {
		if (response.payload(i).result().status() != Plugin::Common_Status_StatusType_STATUS_OK)
			nscapi::plugin_singleton->get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Failed to register " + channel + ": " + response.payload(i).result().message());
	}
}

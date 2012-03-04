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

extern nscapi::helper_singleton* nscapi::plugin_singleton;

nscapi::core_wrapper* get_core() {
	return nscapi::plugin_singleton->get_core();
}

bool nscapi::core_helper::submit_simple_message(std::wstring channel, std::wstring command, NSCAPI::nagiosReturn code, std::wstring & message, std::wstring & perf, std::wstring & response) {
	std::string request, buffer;
	nscapi::functions::create_simple_submit_request(channel, command, code, message, perf, request);
	NSCAPI::nagiosReturn ret = get_core()->submit_message(channel, request, buffer);
	if (ret == NSCAPI::returnIgnored) {
		response = _T("No handler for this message");
		return false;
	}
	if (buffer.size() == 0) {
		response = _T("Missing response from submission");
		return false;
	}
	nscapi::functions::parse_simple_submit_response(buffer, response);
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
NSCAPI::nagiosReturn nscapi::core_helper::simple_query(const std::wstring command, const std::list<std::wstring> & argument, std::wstring & msg, std::wstring & perf) 
{
	std::string response;
	NSCAPI::nagiosReturn ret = simple_query(command, argument, response);
	if (!response.empty()) {
		try {
			return nscapi::functions::parse_simple_query_response(response, msg, perf);
		} catch (std::exception &e) {
			CORE_LOG_ERROR(_T("Failed to extract return message: ") + utf8::cvt<std::wstring>(e.what()));
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
NSCAPI::nagiosReturn nscapi::core_helper::simple_query(const std::wstring command, const std::list<std::wstring> & arguments, std::string & result) 
{
	std::string request;
	try {
		nscapi::functions::create_simple_query_request(command, arguments, request);
	} catch (std::exception &e) {
		CORE_LOG_ERROR(_T("Failed to extract return message: ") + utf8::cvt<std::wstring>(e.what()));
		return NSCAPI::returnUNKNOWN;
	}
	return get_core()->query(command.c_str(), request, result);
}

NSCAPI::nagiosReturn nscapi::core_helper::simple_query_from_nrpe(const std::wstring command, const std::wstring & buffer, std::wstring & message, std::wstring & perf) {
	boost::tokenizer<boost::char_separator<wchar_t>, std::wstring::const_iterator, std::wstring > tok(buffer, boost::char_separator<wchar_t>(_T("!")));
	std::list<std::wstring> arglist;
	BOOST_FOREACH(std::wstring s, tok)
		arglist.push_back(s);
	return simple_query(command, arglist, message, perf);
}

NSCAPI::nagiosReturn nscapi::core_helper::exec_simple_command(const std::wstring target, const std::wstring command, const std::list<std::wstring> &argument, std::list<std::wstring> & result) {
	std::string request, response;
	nscapi::functions::create_simple_exec_request(command, argument, request);
	NSCAPI::nagiosReturn ret = get_core()->exec_command(target, command, request, response);
	nscapi::functions::parse_simple_exec_result(response, result);
	return ret;
}

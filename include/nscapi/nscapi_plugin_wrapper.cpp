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

#include <nscapi/nscapi_plugin_wrapper.hpp>
#include <nscapi/nscapi_helper.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>

#include <nscapi/functions.hpp>
#include <nscapi/macros.hpp>
#include <settings/macros.h>
#include <arrayBuffer.h>
//#include <config.h>
#include <strEx.h>

#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/tokenizer.hpp>

#include <protobuf/plugin.pb.h>

using namespace nscp::helpers;


nscapi::helper_singleton::helper_singleton() : core_(new nscapi::core_wrapper()), plugin_(new nscapi::plugin_wrapper()) {}

/**
 * Used to help store the module handle (and possibly other things in the future)
 * @param hModule cf. DllMain
 * @param ul_reason_for_call cf. DllMain
 * @return TRUE
 */
#ifdef WIN32
int nscapi::plugin_wrapper::wrapDllMain(HANDLE hModule, DWORD ul_reason_for_call)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
		hModule_ = (HINSTANCE)hModule;
		break;
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
#endif
/**
 * Wrapper function around the ModuleHelperInit call.
 * This wrapper retrieves all pointers and stores them for future use.
 * @param f A function pointer to a function that can be used to load function from the core.
 * @return NSCAPI::success or NSCAPI::failure
 */
int nscapi::plugin_wrapper::wrapModuleHelperInit(unsigned int id, nscapi::core_api::lpNSAPILoader f) {
	return GET_CORE()->load_endpoints(f)?NSCAPI::isSuccess:NSCAPI::hasFailed;
}


void nscapi::impl::simple_log_handler::handleMessageRAW(std::string data) {
	try {
		Plugin::LogEntry message;
		message.ParseFromString(data);

		for (int i=0;i<message.entry_size();i++) {
			Plugin::LogEntry::Entry msg = message.entry(i);
			handleMessage(msg.level(), msg.file(), msg.line(), msg.message());
		}
	} catch (std::exception &e) {
		std::cout << "Failed to parse data from: " << strEx::strip_hex(data) << e.what() <<  std::endl;;
	} catch (...) {
		std::cout << "Failed to parse data from: " << strEx::strip_hex(data) << std::endl;;
	}
}

NSCAPI::nagiosReturn nscapi::impl::simple_command_handler::handleRAWCommand(const wchar_t* char_command, const std::string &request, std::string &response) {
	nscapi::functions::decoded_simple_command_data data = nscapi::functions::parse_simple_query_request(char_command, request);
	std::wstring msg, perf;

	NSCAPI::nagiosReturn ret = handleCommand(data.target, boost::algorithm::to_lower_copy(data.command), data.args, msg, perf);
	nscapi::functions::create_simple_query_response(data.command, ret, msg, perf, response);
	return ret;
}

NSCAPI::nagiosReturn nscapi::impl::simple_command_line_exec::commandRAWLineExec(const wchar_t* char_command, const std::string &request, std::string &response) {
	nscapi::functions::decoded_simple_command_data data = nscapi::functions::parse_simple_exec_request(char_command, request);
	std::wstring result;
	NSCAPI::nagiosReturn ret = commandLineExec(data.command, data.args, result);
	if (ret == NSCAPI::returnIgnored)
		return NSCAPI::returnIgnored;
	nscapi::functions::create_simple_exec_response(data.command, ret, result, response);
	return ret;
}

NSCAPI::nagiosReturn nscapi::impl::simple_submission_handler::handleRAWNotification(const wchar_t* channel, std::string request, std::string &response) {
	try {
		std::wstring source, command, msg, perf;
		int code = nscapi::functions::parse_simple_submit_request(request, source, command, msg, perf);
		NSCAPI::nagiosReturn ret = handleSimpleNotification(channel, source, command, code, msg, perf);
		if (ret == NSCAPI::returnIgnored)
			return NSCAPI::returnIgnored;
		nscapi::functions::create_simple_submit_response(channel, command, ret, _T(""), response);
	} catch (std::exception &e) {
		nscapi::plugin_singleton->get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, utf8::cvt<std::wstring>("Failed to parse data from: " + strEx::strip_hex(request) + ": " + e.what()));
	} catch (...) {
		nscapi::plugin_singleton->get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, utf8::cvt<std::wstring>("Failed to parse data from: " + strEx::strip_hex(request)));
	}
	return NSCAPI::returnIgnored;
}
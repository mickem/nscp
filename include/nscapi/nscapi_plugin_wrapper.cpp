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

#include <boost/foreach.hpp>
#include <boost/tokenizer.hpp>

#include <protobuf/plugin.pb.h>

using namespace nscp::helpers;

extern nscapi::helper_singleton* nscapi::plugin_singleton;
/**
* Wrap a return string.
* This function copies a string to a char buffer making sure the buffer has the correct length.
*
* @param *buffer Buffer to copy the string to.
* @param bufLen Length of the buffer
* @param str Th string to copy
* @param defaultReturnCode The default return code
* @return NSCAPI::success unless the buffer is to short then it will be NSCAPI::invalidBufferLen
*/
int nscapi::plugin_wrapper::wrapReturnString(wchar_t *buffer, unsigned int bufLen, std::wstring str, int defaultReturnCode ) {
	// @todo deprecate this
	if (str.length() >= bufLen) {
		std::wstring sstr = str.substr(0, bufLen-2);
		NSC_DEBUG_MSG_STD(_T("String (") + strEx::itos(str.length()) + _T(") to long to fit inside buffer(") + strEx::itos(bufLen) + _T(") : ") + sstr);
		return NSCAPI::isInvalidBufferLen;
	}
	wcsncpy(buffer, str.c_str(), bufLen);
	return defaultReturnCode;
}

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
	return nscapi::plugin_singleton->get_core()->load_endpoints(id, f)?NSCAPI::isSuccess:NSCAPI::hasFailed;
}
/**
* Wrap the GetModuleName function call
* @param buf Buffer to store the module name
* @param bufLen Length of buffer
* @param str String to store inside the buffer
* @	 copy status
*/
NSCAPI::errorReturn nscapi::plugin_wrapper::wrapGetModuleName(wchar_t* buf, unsigned int bufLen, std::wstring str) {
	return nscapi::plugin_wrapper::wrapReturnString(buf, bufLen, str, NSCAPI::isSuccess);
}

/**
 * Wrap the GetModuleVersion function call
 * @param *major Major version number
 * @param *minor Minor version number
 * @param *revision Revision
 * @param version version as a module_version
 * @return NSCAPI::success
 */
NSCAPI::errorReturn nscapi::plugin_wrapper::wrapGetModuleVersion(int *major, int *minor, int *revision, module_version version) {
	*major = version.major;
	*minor = version.minor;
	*revision = version.revision;
	return NSCAPI::isSuccess;
}
/**
 * Wrap the HasCommandHandler function call
 * @param has true if this module has a command handler
 * @return NSCAPI::istrue or NSCAPI::isfalse
 */
NSCAPI::boolReturn nscapi::plugin_wrapper::wrapHasCommandHandler(bool has) {
	return has?NSCAPI::istrue:NSCAPI::isfalse;
}
/**
 * Wrap the HasMessageHandler function call
 * @param has true if this module has a message handler
 * @return NSCAPI::istrue or NSCAPI::isfalse
 */
NSCAPI::boolReturn nscapi::plugin_wrapper::wrapHasMessageHandler(bool has) {
	return has?NSCAPI::istrue:NSCAPI::isfalse;
}
NSCAPI::boolReturn nscapi::plugin_wrapper::wrapHasNotificationHandler(bool has) {
	return has?NSCAPI::istrue:NSCAPI::isfalse;
}

NSCAPI::nagiosReturn nscapi::plugin_wrapper::wrapHandleNotification(NSCAPI::nagiosReturn retResult) {
	return retResult;
}

/**
 * Wrap the HandleCommand call
 * @param retResult The returned result
 * @param retMessage The returned message
 * @param retPerformance The returned performance data
 * @param *returnBufferMessage The return message buffer
 * @param returnBufferMessageLen The return message buffer length
 * @param *returnBufferPerf The return performance data buffer
 * @param returnBufferPerfLen The return performance data buffer length
 * @return the return code
 */
NSCAPI::nagiosReturn nscapi::plugin_wrapper::wrapHandleCommand(NSCAPI::nagiosReturn retResult, const std::string &reply, char **reply_buffer, unsigned int *size) {
	// TODO: Make this global to allow remote deletion!!!
	unsigned int buf_len = reply.size();
	*reply_buffer = new char[buf_len + 10];
	memcpy(*reply_buffer, reply.c_str(), buf_len+1);
	(*reply_buffer)[buf_len] = 0;
	(*reply_buffer)[buf_len+1] = 0;
	*size = buf_len;
	if (!nscapi::plugin_helper::isMyNagiosReturn(retResult)) {
		NSC_LOG_ERROR(_T("A module returned an invalid return code"));
	}
	return retResult;
}
NSCAPI::nagiosReturn nscapi::plugin_wrapper::wrapCommandLineExec(NSCAPI::nagiosReturn retResult, const std::string &reply, char **reply_buffer, unsigned int *size) {
	// TODO: Make this global to allow remote deletion!!!
	unsigned int buf_len = reply.size();
	*reply_buffer = new char[buf_len + 10];
	memcpy(*reply_buffer, reply.c_str(), buf_len+1);
	(*reply_buffer)[buf_len] = 0;
	(*reply_buffer)[buf_len+1] = 0;
	*size = buf_len;
	return retResult;
}

/**
 * Wrap the NSLoadModule call
 * @param success true if module load was successfully
 * @return NSCAPI::success or NSCAPI::failed
 */
int nscapi::plugin_wrapper::wrapLoadModule(bool success) {
	if (success)
		return NSCAPI::isSuccess;
	return NSCAPI::hasFailed;
}
/**
 * Wrap the NSUnloadModule call
 * @param success true if module load was successfully
 * @return NSCAPI::success or NSCAPI::failed
 */
int nscapi::plugin_wrapper::wrapUnloadModule(bool success) {
	if (success)
		return NSCAPI::isSuccess;
	return NSCAPI::hasFailed;
}
void nscapi::plugin_wrapper::wrapDeleteBuffer(char**buffer) {
	delete [] *buffer;
}


nscapi::helper_singleton::helper_singleton() : core_(new nscapi::core_wrapper()), plugin_(new nscapi::plugin_wrapper()) {}


/*
NSCAPI::nagiosReturn nscapi::impl::CommandImpl::handleRAWCommand(const wchar_t* char_command, const std::string &request, std::string &response) {

	nscapi::functions::parse_simple_query_request(char_command, request);
	std::wstring command = char_command;
	PluginCommand::RequestMessage request_message;
	request_message.ParseFromString(request);

	if (request_message.payload_size() != 1) {
		return NSCAPI::returnIgnored;
	}
	::PluginCommand::Request req_payload = request_message.payload().Get(0);

	PluginCommand::ResponseMessage response_message;
	::PluginCommand::Header* hdr = response_message.mutable_header();

	hdr->set_type(PluginCommand::Header_Type_RESPONSE);
	hdr->set_version(PluginCommand::Header_Version_VERSION_1);

	PluginCommand::Response *resp_payload = response_message.add_payload();

	handleCommand(command, &req_payload, resp_payload);


	resp_payload->set_version(PluginCommand::Response_Version_VERSION_1);
	response_message.SerializeToString(&response);

	return NSCAPI::returnOK;
}
*/
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

NSCAPI::nagiosReturn nscapi::impl::simple_command::handleRAWCommand(const wchar_t* char_command, const std::string &request, std::string &response) {
	nscapi::functions::decoded_simple_command_data data = nscapi::functions::parse_simple_query_request(char_command, request);
	std::wstring msg, perf;
	NSCAPI::nagiosReturn ret = handleCommand(data.target, data.command, data.args, msg, perf);
	nscapi::functions::create_simple_query_response(ret, msg, perf, response, data.command);
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

NSCAPI::nagiosReturn nscapi::impl::SimpleNotificationHandler::handleRAWNotification(const wchar_t* channel, const wchar_t* command, NSCAPI::nagiosReturn code, std::string result) {
	try {
		std::wstring msg, perf;
		nscapi::functions::parse_simple_query_response(result, msg, perf);
		return handleSimpleNotification(channel, command, code, msg, perf);
	} catch (std::exception &e) {
		nscapi::plugin_singleton->get_core()->Message(NSCAPI::error, __FILE__, __LINE__, utf8::cvt<std::wstring>("Failed to parse data from: " + strEx::strip_hex(result) + ": " + e.what()));
	} catch (...) {
		nscapi::plugin_singleton->get_core()->Message(NSCAPI::error, __FILE__, __LINE__, utf8::cvt<std::wstring>("Failed to parse data from: " + strEx::strip_hex(result)));
	}
	return NSCAPI::returnUNKNOWN;
}
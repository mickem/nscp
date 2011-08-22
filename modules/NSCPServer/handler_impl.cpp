#include "stdafx.h"

#include <boost/asio.hpp>
#include <protobuf/plugin.pb.h>
#include <nscapi/functions.hpp>

#include "handler_impl.hpp"

std::string handler_impl::process(std::string &buffer) {
	std::string ret;
	PluginCommand::RequestMessage msg;
	msg.ParseFromString(buffer);
	if (msg.payload_size() != 1) {
		NSC_LOG_ERROR(_T("Multiple payloads not currently supported..."));
		nscapi::functions::create_simple_query_result(NSCAPI::returnUNKNOWN, _T("Multiple payloads not currently supported..."), _T(""), ret);
		return ret;
	}
	std::wstring command = utf8::cvt<std::wstring>(msg.payload(0).command());
	if (command.empty() || command == _T("_NSCP_CHECK")) {
		nscapi::functions::create_simple_query_result(NSCAPI::returnOK, _T("I (") + nscapi::plugin_singleton->get_core()->getApplicationVersionString() + _T(") seem to be doing fine..."), _T(""), ret);
		return ret;
	}

	// @todo re-add support for nasty arguments / arguments passing
	/*
	if (!allowNasty_) {
		if (cmd.first.find_first_of(NASTY_METACHARS) != std::wstring::npos) {
			NSC_LOG_ERROR(_T("Request command contained illegal metachars!"));
			throw nscp::nscp_exception(_T("Request command contained illegal metachars!"));
		}
		if (cmd.second.find_first_of(NASTY_METACHARS) != std::wstring::npos) {
			NSC_LOG_ERROR(_T("Request arguments contained illegal metachars!"));
			throw nscp::nscp_exception(_T("Request command contained illegal metachars!"));
		}
	}
	*/
	try {
		NSC_DEBUG_MSG_STD(_T("Running command: ") + command);
		NSCAPI::nagiosReturn returncode = nscapi::plugin_singleton->get_core()->query(command, buffer, ret);
	} catch (...) {
		NSC_LOG_ERROR(_T("Internal exception processing request: ") + command);
		nscapi::functions::create_simple_query_result(NSCAPI::returnUNKNOWN, _T("Internal exception processing request: ") + command, _T(""), ret);
		return ret;
	}
	return ret;
}





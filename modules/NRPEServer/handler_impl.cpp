#include "stdafx.h"

#include <boost/asio.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include "handler_impl.hpp"
#include <config.h>

nrpe::packet handler_impl::handle(nrpe::packet p) {
	strEx::token cmd = strEx::getToken(p.getPayload(), '!');
	if (cmd.first == _T("_NRPE_CHECK")) {
		return nrpe::packet::create_response(NSCAPI::returnOK, _T("I (") + nscapi::plugin_singleton->get_core()->getApplicationVersionString() + _T(") seem to be doing fine..."), p.get_payload_length());
	}
	std::wstring msg, perf;

	if (!allowArgs_) {
		if (!cmd.second.empty()) {
			NSC_LOG_ERROR(_T("Request contained arguments (not currently allowed, check the allow_arguments option)."));
			throw nrpe::nrpe_exception(_T("Request contained arguments (not currently allowed, check the allow_arguments option)."));
		}
	}
	if (!allowNasty_) {
		if (cmd.first.find_first_of(NASTY_METACHARS_W) != std::wstring::npos) {
			NSC_LOG_ERROR(_T("Request command contained illegal metachars!"));
			throw nrpe::nrpe_exception(_T("Request command contained illegal metachars!"));
		}
		if (cmd.second.find_first_of(NASTY_METACHARS_W) != std::wstring::npos) {
			NSC_LOG_ERROR(_T("Request arguments contained illegal metachars!"));
			throw nrpe::nrpe_exception(_T("Request command contained illegal metachars!"));
		}
	}

	NSCAPI::nagiosReturn ret = -3;
	try {
		NSC_DEBUG_MSG_STD(_T("Running command: ") + cmd.first);
		ret = nscapi::core_helper::simple_query_from_nrpe(cmd.first, cmd.second, msg, perf);
		NSC_DEBUG_MSG_STD(_T("Running command: ") + cmd.first + _T(" = ") + msg);
	} catch (...) {
		return nrpe::packet::create_response(NSCAPI::returnUNKNOWN, _T("UNKNOWN: Internal exception"), p.get_payload_length());
	}
	switch (ret) {
	case NSCAPI::returnInvalidBufferLen:
		return nrpe::packet::create_response(NSCAPI::returnUNKNOWN, _T("UNKNOWN: Return buffer to small to handle this command."), p.get_payload_length());
	case NSCAPI::returnIgnored:
		return nrpe::packet::create_response(NSCAPI::returnUNKNOWN, _T("UNKNOWN: No handler for that command."), p.get_payload_length());
	case NSCAPI::returnOK:
	case NSCAPI::returnWARN:
	case NSCAPI::returnCRIT:
	case NSCAPI::returnUNKNOWN:
		break;
	default:
		return nrpe::packet::create_response(NSCAPI::returnUNKNOWN, _T("UNKNOWN: Internal error."), p.get_payload_length());
	}
	std::wstring data;
	if (msg.length() > p.get_payload_length()) {
		data = msg.substr(0, p.get_payload_length()-1);
	} else if (perf.empty() || noPerfData_) {
		data = msg;
	} else if (msg.length() + perf.length() + 1 > p.get_payload_length()) {
		data = msg;
	} else {
		data = msg + _T("|") + perf;
	}
	return nrpe::packet::create_response(ret, data, p.get_payload_length());
}





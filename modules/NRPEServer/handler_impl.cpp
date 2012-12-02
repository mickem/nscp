#include "stdafx.h"

#include <boost/asio.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include "handler_impl.hpp"
#include <config.h>

nrpe::packet handler_impl::handle(nrpe::packet p) {
	strEx::s::token cmd = strEx::s::getToken(p.getPayload(), '!');
	if (cmd.first == "_NRPE_CHECK") {
		return nrpe::packet::create_response(NSCAPI::returnOK, "I (" + utf8::cvt<std::string>(nscapi::plugin_singleton->get_core()->getApplicationVersionString()) + ") seem to be doing fine...", p.get_payload_length());
	}
	if (!allowArgs_) {
		if (!cmd.second.empty()) {
			NSC_LOG_ERROR(_T("Request contained arguments (not currently allowed, check the allow arguments option)."));
			throw nrpe::nrpe_exception("Request contained arguments (not currently allowed, check the allow arguments option).");
		}
	}
	if (!allowNasty_) {
		if (cmd.first.find_first_of(NASTY_METACHARS) != std::wstring::npos) {
			NSC_LOG_ERROR(_T("Request command contained illegal metachars!"));
			throw nrpe::nrpe_exception("Request command contained illegal metachars!");
		}
		if (cmd.second.find_first_of(NASTY_METACHARS) != std::wstring::npos) {
			NSC_LOG_ERROR(_T("Request arguments contained illegal metachars!"));
			throw nrpe::nrpe_exception("Request command contained illegal metachars!");
		}
	}

	std::wstring wmsg, wperf;
	NSCAPI::nagiosReturn ret = -3;
	try {
		ret = nscapi::core_helper::simple_query_from_nrpe(utf8::cvt<std::wstring>(cmd.first), utf8::cvt<std::wstring>(cmd.second), wmsg, wperf);
	} catch (...) {
		return nrpe::packet::create_response(NSCAPI::returnUNKNOWN, "UNKNOWN: Internal exception", p.get_payload_length());
	}
	switch (ret) {
	case NSCAPI::returnInvalidBufferLen:
		return nrpe::packet::create_response(NSCAPI::returnUNKNOWN, "UNKNOWN: Return buffer to small to handle this command.", p.get_payload_length());
	case NSCAPI::returnIgnored:
		return nrpe::packet::create_response(NSCAPI::returnUNKNOWN, "UNKNOWN: No handler for that command.", p.get_payload_length());
	case NSCAPI::returnOK:
	case NSCAPI::returnWARN:
	case NSCAPI::returnCRIT:
	case NSCAPI::returnUNKNOWN:
		break;
	default:
		return nrpe::packet::create_response(NSCAPI::returnUNKNOWN, "UNKNOWN: Internal error.", p.get_payload_length());
	}
	std::string data;
	std::string msg = utf8::cvt<std::string>(wmsg);
	std::string perf = utf8::cvt<std::string>(wperf);

	if (msg.length() > p.get_payload_length()) {
		data = msg.substr(0, p.get_payload_length()-1);
	} else if (perf.empty() || noPerfData_) {
		data = msg;
	} else if (msg.length() + perf.length() + 1 > p.get_payload_length()) {
		data = msg;
	} else {
		data = msg + "|" + perf;
	}
	return nrpe::packet::create_response(ret, data, p.get_payload_length());
}





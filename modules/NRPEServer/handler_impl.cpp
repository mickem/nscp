#include "stdafx.h"

#include <boost/asio.hpp>
#include "handler_impl.hpp"

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
		if (cmd.first.find_first_of(NASTY_METACHARS) != std::wstring::npos) {
			NSC_LOG_ERROR(_T("Request command contained illegal metachars!"));
			throw nrpe::nrpe_exception(_T("Request command contained illegal metachars!"));
		}
		if (cmd.second.find_first_of(NASTY_METACHARS) != std::wstring::npos) {
			NSC_LOG_ERROR(_T("Request arguments contained illegal metachars!"));
			throw nrpe::nrpe_exception(_T("Request command contained illegal metachars!"));
		}
	}
	//TODO REMOVE THIS
	//return NRPEPacket(NRPEPacket::responsePacket, NRPEPacket::version2, NSCAPI::returnUNKNOWN, _T("TEST TEST TEST"), buffer_length_);

	NSCAPI::nagiosReturn ret = -3;
	try {
		NSC_DEBUG_MSG_STD(_T("Running command: ") + cmd.first);
		ret = nscapi::plugin_singleton->get_core()->InjectNRPECommand(cmd.first, cmd.second, msg, perf);
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
	std::wstring data = msg;
	if (!perf.empty()&&!noPerfData_) {
		data += _T("|") + perf;
	}
	if (data.length() >= p.get_payload_length()-1) {
		NSC_LOG_ERROR(_T("Truncating returndata as it is bigger then NRPE allowes :("));
		data = data.substr(0,p.get_payload_length()-2);
	}
	return nrpe::packet::create_response(ret, data, p.get_payload_length());
}





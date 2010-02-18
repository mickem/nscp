#include "stdafx.h"
#include <boost/asio.hpp>
#include "nrpe_handler.hpp"

namespace nrpe {
	namespace server {
		nrpe::packet handler::handle(nrpe::packet p) {
			strEx::token cmd = strEx::getToken(p.getPayload(), '!');
			if (cmd.first == _T("_NRPE_CHECK")) {
				return nrpe::packet::create_response(NSCAPI::returnOK, _T("I (") + nscapi::plugin_singleton->get_core()->getApplicationVersionString() + _T(") seem to be doing fine..."), p.get_payload_length());
			}
			std::wstring msg, perf;

			if (allowArgs_) {
				if (!cmd.second.empty()) {
					NSC_LOG_ERROR(_T("Request contained arguments (not currently allowed, check the allow_arguments option)."));
					throw nrpe::nrpe_exception(_T("Request contained arguments (not currently allowed, check the allow_arguments option)."));
				}
			}
			if (allowNasty_) {
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
				ret = nscapi::plugin_singleton->get_core()->InjectSplitAndCommand(cmd.first, cmd.second, '!', msg, perf);
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
			if (msg.length() >= p.get_payload_length()-1) {
				NSC_LOG_ERROR(_T("Truncating returndata as it is bigger then NRPE allowes :("));
				msg = msg.substr(0,p.get_payload_length()-2);
			}
			if (perf.empty()||noPerfData_) {
				return nrpe::packet::create_response(ret, msg, p.get_payload_length());
			} else {
				return nrpe::packet::create_response(ret, msg + _T("|") + perf, p.get_payload_length());
			}
		}
	}
}





#include "stdafx.h"
#include <common.hpp>

#include <nscapi/nscapi_core_helper.hpp>
#include "handler_impl.hpp"

std::list<nrpe::packet> handler_impl::handle(nrpe::packet p) {
	std::list<nrpe::packet> packets;
	strEx::s::token cmd = strEx::s::getToken(p.getPayload(), '!');
	if (cmd.first == "_NRPE_CHECK") {
		packets.push_back(nrpe::packet::create_response(NSCAPI::returnOK, "I (" + utf8::cvt<std::string>(nscapi::plugin_singleton->get_core()->getApplicationVersionString()) + ") seem to be doing fine...", p.get_payload_length()));
		return packets;
	}
	if (!allowArgs_) {
		if (!cmd.second.empty()) {
			NSC_LOG_ERROR("Request contained arguments (not currently allowed, check the allow arguments option).");
			throw nrpe::nrpe_exception("Request contained arguments (not currently allowed, check the allow arguments option).");
		}
	}
	if (!allowNasty_) {
		if (cmd.first.find_first_of(NASTY_METACHARS) != std::wstring::npos) {
			NSC_LOG_ERROR("Request command contained illegal metachars!");
			throw nrpe::nrpe_exception("Request command contained illegal metachars!");
		}
		if (cmd.second.find_first_of(NASTY_METACHARS) != std::wstring::npos) {
			NSC_LOG_ERROR("Request arguments contained illegal metachars!");
			throw nrpe::nrpe_exception("Request command contained illegal metachars!");
		}
	}
	std::string wmsg, wperf;
	NSCAPI::nagiosReturn ret = -3;
	try {
		if (encoding_.empty()) {
			ret = nscapi::core_helper::simple_query_from_nrpe(utf8::cvt<std::string>(utf8::to_unicode(cmd.first)), utf8::cvt<std::string>(utf8::to_unicode(cmd.second)), wmsg, wperf);
		} else {
			ret = nscapi::core_helper::simple_query_from_nrpe(utf8::cvt<std::string>(utf8::from_encoding(cmd.first, encoding_)), utf8::cvt<std::string>(utf8::from_encoding(cmd.second, encoding_)), wmsg, wperf);
		}
	} catch (...) {
		packets.push_back(nrpe::packet::create_response(NSCAPI::returnUNKNOWN, "UNKNOWN: Internal exception", p.get_payload_length()));
		return packets;
	}
	switch (ret) {
	case NSCAPI::returnInvalidBufferLen:
		throw nrpe::nrpe_exception("UNKNOWN: Return buffer to small to handle this command.");
	case NSCAPI::returnIgnored:
		throw nrpe::nrpe_exception("UNKNOWN: No handler for that command.");
	case NSCAPI::returnOK:
	case NSCAPI::returnWARN:
	case NSCAPI::returnCRIT:
	case NSCAPI::returnUNKNOWN:
		break;
	default:
		throw nrpe::nrpe_exception("UNKNOWN: Internal error.");
	}
	std::string data,msg, perf;
	if (encoding_.empty()) {
		msg = utf8::to_system(utf8::cvt<std::wstring>(wmsg));
		perf = utf8::to_system(utf8::cvt<std::wstring>(wperf));
	} else {
		msg = utf8::to_encoding(utf8::cvt<std::wstring>(wmsg), encoding_);
		perf = utf8::to_encoding(utf8::cvt<std::wstring>(wperf), encoding_);
	}
	const unsigned int max_len = p.get_payload_length()-1;
	if (multiple_packets_) {
		data = msg + "|" + perf;
		std::size_t data_len = data.size();
		for (std::size_t i = 0; i < data_len; i+=max_len) {
			if (i > data_len - max_len)
				packets.push_back(nrpe::packet::create_response(ret, data.substr(i, max_len), p.get_payload_length()));
			else
				packets.push_back(nrpe::packet::create_more_response(ret, data.substr(i, max_len), p.get_payload_length()));
		}
	} else {
		if (msg.length() >= max_len) {
			data = msg.substr(0, max_len);
		} else if (perf.empty() || noPerfData_) {
			data = msg;
		} else if (msg.length() + perf.length() + 1 > max_len) {
			data = msg;
		} else {
			data = msg + "|" + perf;
		}
		packets.push_back(nrpe::packet::create_response(ret, data, p.get_payload_length()));
	}
	return packets;
}





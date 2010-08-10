#pragma once

#include <nrpe/packet.hpp>
#include <nrpe/server/handler.hpp>
#include <boost/tuple/tuple.hpp>

class handler_impl : public nrpe::server::handler, private boost::noncopyable {
	unsigned int payload_length_;
	bool allowArgs_;
	bool allowNasty_;
	bool noPerfData_;
public:
	handler_impl(unsigned int payload_length) : payload_length_(payload_length), noPerfData_(false) {}

	unsigned int get_payload_length() {
		return payload_length_;
	}
	void set_payload_length(unsigned int payload) {
		payload_length_ = payload;
	}

	nrpe::packet handle(nrpe::packet packet);

	nrpe::packet create_error(std::wstring msg) {
		return nrpe::packet::create_response(4, msg, payload_length_);
	}

	void log_debug(std::wstring file, int line, std::wstring msg) {
		GET_CORE()->Message(NSCAPI::debug, file, line, msg);
	}
	void log_error(std::wstring file, int line, std::wstring msg) {
		GET_CORE()->Message(NSCAPI::error, file, line, msg);
	}
};

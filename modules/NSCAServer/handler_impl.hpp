#pragma once

#include <boost/tuple/tuple.hpp>

#include <nsca/nsca_packet.hpp>
#include <nsca/server/handler.hpp>

#include <unicode_char.hpp>

class handler_impl : public nsca::server::handler, private boost::noncopyable {
	unsigned int payload_length_;
	bool allowArgs_;
	bool allowNasty_;
	bool noPerfData_;
	std::wstring channel_;
public:
	handler_impl(unsigned int payload_length) : payload_length_(payload_length), noPerfData_(false), allowNasty_(false), allowArgs_(false) {}

	unsigned int get_payload_length() {
		return payload_length_;
	}
	void set_payload_length(unsigned int payload) {
		payload_length_ = payload;
	}
	void set_channel(std::wstring channel) {
		channel_ = channel;
	}
	std::wstring get_channel() {
		return channel_;
	}

	void handle(nsca::packet packet);

	virtual void set_perf_data(bool v) {
		noPerfData_ = !v;
		if (noPerfData_)
			log_debug(__FILE__, __LINE__, _T("Performance data disabled!"));
	}

	void log_debug(std::string file, int line, std::wstring msg) {
		GET_CORE()->Message(NSCAPI::debug, file, line, msg);
	}
	void log_error(std::string file, int line, std::wstring msg) {
		GET_CORE()->Message(NSCAPI::error, file, line, msg);
	}
};

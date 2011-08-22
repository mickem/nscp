#pragma once

#include <nscp/packet.hpp>
#include <nscp/server/handler.hpp>
#include <boost/tuple/tuple.hpp>

class handler_impl : public nscp::server::handler, private boost::noncopyable {
	unsigned int payload_length_;
	bool allowArgs_;
	bool allowNasty_;
	bool noPerfData_;
public:
	handler_impl(unsigned int payload_length) : payload_length_(payload_length), noPerfData_(false), allowNasty_(false), allowArgs_(false) {}

	unsigned int get_payload_length() {
		return payload_length_;
	}
	void set_payload_length(unsigned int payload) {
		payload_length_ = payload;
	}

	std::string process(std::string &buffer);

	nscp::packet create_error(std::wstring msg) {
		return nscp::packet::create_response(4, msg, payload_length_);
	}

	virtual void set_allow_arguments(bool v)  {
		allowArgs_ = v;
	}
	virtual void set_allow_nasty_arguments(bool v) {
		allowNasty_ = v;
	}
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

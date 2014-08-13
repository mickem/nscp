#pragma once

#include <nscapi/nscapi_helper_singleton.hpp>

#include <nrpe/packet.hpp>
#include <nrpe/server/handler.hpp>
#include <boost/tuple/tuple.hpp>

class handler_impl : public nrpe::server::handler {
	unsigned int payload_length_;
	bool noPerfData_;
	bool allowNasty_;
	bool allowArgs_;
	bool multiple_packets_;
public:
	std::string encoding_;
	handler_impl(unsigned int payload_length) 
		: payload_length_(payload_length)
		, noPerfData_(false)
		, allowNasty_(false)
		, allowArgs_(false)
		, multiple_packets_(false)
	{}

	unsigned int get_payload_length() {
		return payload_length_;
	}
	void set_payload_length(unsigned int payload) {
		payload_length_ = payload;
	}

	std::list<nrpe::packet> handle(nrpe::packet packet);

	nrpe::packet create_error(std::string msg) {
		return nrpe::packet::create_response(3, msg, payload_length_);
	}

	virtual void set_allow_arguments(bool v)  {
		allowArgs_ = v;
	}
	virtual void set_allow_nasty_arguments(bool v) {
		allowNasty_ = v;
	}
	virtual void set_multiple_packets(bool v) {
		multiple_packets_ = v;
	}
	virtual void set_perf_data(bool v) {
		noPerfData_ = !v;
		if (noPerfData_)
			log_debug("nrpe", __FILE__, __LINE__, "Performance data disabled!");
	}

	void log_debug(std::string module, std::string file, int line, std::string msg) const {
		if (nscapi::plugin_singleton->get_core()->should_log(NSCAPI::log_level::debug)) {
			nscapi::plugin_singleton->get_core()->log(NSCAPI::log_level::debug, file, line, msg);
		}
	}
	void log_error(std::string module, std::string file, int line, std::string msg) const {
		if (nscapi::plugin_singleton->get_core()->should_log(NSCAPI::log_level::error)) {
			nscapi::plugin_singleton->get_core()->log(NSCAPI::log_level::error, file, line, msg);
		}
	}
};

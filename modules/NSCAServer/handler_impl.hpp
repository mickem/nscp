#pragma once

#include <boost/tuple/tuple.hpp>

#include <nscapi/nscapi_plugin_interface.hpp>

#include <nsca/nsca_packet.hpp>
#include <cryptopp/cryptopp.hpp>
#include <nsca/server/handler.hpp>

class nsca_handler_impl : public nsca::server::handler {
	unsigned int payload_length_;
	bool noPerfData_;
	bool allowNasty_;
	bool allowArgs_;

	std::string channel_;
	int encryption_;
	std::string password_;
public:
	nsca_handler_impl(unsigned int payload_length) 
		: payload_length_(payload_length)
		, noPerfData_(false)
		, allowNasty_(false)
		, allowArgs_(false) {}

	unsigned int get_payload_length() {
		return payload_length_;
	}
	void set_payload_length(unsigned int payload) {
		payload_length_ = payload;
	}
	void set_channel(std::string channel) {
		channel_ = channel;
	}
	std::string get_channel() {
		return channel_;
	}
	void set_encryption(std::string enc) {
		encryption_ = nscp::encryption::helpers::encryption_to_int(enc);
	}
	int get_encryption() {
		return encryption_;
	}
	std::string get_password() {
		return password_;
	}
	void set_password(std::string pwd) {
		password_ = pwd;
	}

	void handle(nsca::packet packet);

	virtual void set_perf_data(bool v) {
		noPerfData_ = !v;
		if (noPerfData_)
			log_debug("nsca", __FILE__, __LINE__, "Performance data disabled!");
	}

	void log_debug(std::string module, std::string file, int line, std::string msg) const {
		if (GET_CORE()->should_log(NSCAPI::log_level::debug)) {
			GET_CORE()->log(NSCAPI::log_level::debug, file, line, msg);
		}
	}
	void log_error(std::string module, std::string file, int line, std::string msg) const {
		if (GET_CORE()->should_log(NSCAPI::log_level::error)) {
			GET_CORE()->log(NSCAPI::log_level::error, file, line, msg);
		}
	}
};

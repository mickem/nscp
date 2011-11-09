#pragma once

#include <boost/tuple/tuple.hpp>

#include <nsca/nsca_packet.hpp>
#include <nsca/nsca_enrypt.hpp>
#include <nsca/server/handler.hpp>

#include <unicode_char.hpp>

class handler_impl : public nsca::server::handler, private boost::noncopyable {
	unsigned int payload_length_;
	bool allowArgs_;
	bool allowNasty_;
	bool noPerfData_;
	std::wstring channel_;
	int encryption_;
	std::string password_;
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
	void set_encryption(std::string enc) {
		encryption_ = nsca::nsca_encrypt::helpers::encryption_to_int(enc);
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
			log_debug(__FILE__, __LINE__, _T("Performance data disabled!"));
	}

	void log_debug(std::string file, int line, std::wstring msg) {
		GET_CORE()->log(NSCAPI::debug, file, line, msg);
	}
	void log_error(std::string file, int line, std::wstring msg) {
		GET_CORE()->log(NSCAPI::error, file, line, msg);
	}
};

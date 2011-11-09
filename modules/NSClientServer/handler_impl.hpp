#pragma once

#include <check_nt/packet.hpp>
#include <check_nt/server/handler.hpp>
#include <boost/tuple/tuple.hpp>

class handler_impl : public check_nt::server::handler, private boost::noncopyable {
	bool allowArgs_;
	bool allowNasty_;
	bool noPerfData_;
	std::wstring password_;
public:
	handler_impl() : noPerfData_(false), allowNasty_(false), allowArgs_(false) {}

	check_nt::packet handle(check_nt::packet packet);

	check_nt::packet create_error(std::wstring msg) {
		return check_nt::packet("ERROR: Failed to parse");
	}

	virtual void set_allow_arguments(bool v)  {
		allowArgs_ = v;
	}
	virtual void set_allow_nasty_arguments(bool v) {
		allowNasty_ = v;
	}
	virtual void set_perf_data(bool v) {
		noPerfData_ = !v;
	}


	void log_debug(std::string file, int line, std::wstring msg) {
		GET_CORE()->log(NSCAPI::debug, file, line, msg);
	}
	void log_error(std::string file, int line, std::wstring msg) {
		GET_CORE()->log(NSCAPI::error, file, line, msg);
	}

	void set_password(std::wstring password) {
		password_ = password;
	}
	std::wstring get_password() const {
		return password_;
	}

private:
	bool isPasswordOk(std::wstring remotePassword);
};

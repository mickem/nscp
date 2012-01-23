#pragma once

#include <nscp/packet.hpp>
#include <nscp/handler.hpp>
#include <boost/tuple/tuple.hpp>

class handler_impl : private boost::noncopyable, public nscp::handler {
	unsigned int payload_length_;
	bool allowArgs_;
	bool allowNasty_;
	bool noPerfData_;
public:
	handler_impl(unsigned int payload_length) : payload_length_(payload_length), noPerfData_(false), allowNasty_(false), allowArgs_(false) {}

	NSCAPI::nagiosReturn handle_query_request(const std::string &request, Plugin::QueryRequestMessage &msg, std::string &reply);


	NSCAPI::nagiosReturn handle_submission_request(const std::string &,Plugin::SubmitRequestMessage &,std::string &) {return 0;}
	NSCAPI::nagiosReturn handle_exec_request(const std::string &,Plugin::ExecuteRequestMessage &,std::string &) {return 0;}

	nscp::packet create_error(std::wstring msg) {
		return nscp::factory::create_error(msg);
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
		if (GET_CORE()->should_log(NSCAPI::log_level::debug)) {
			GET_CORE()->log(NSCAPI::log_level::debug, file, line, msg);
		}
	}
	void log_error(std::string file, int line, std::wstring msg) {
		if (GET_CORE()->should_log(NSCAPI::log_level::error)) {
			GET_CORE()->log(NSCAPI::log_level::error, file, line, msg);
		}
	}
};
